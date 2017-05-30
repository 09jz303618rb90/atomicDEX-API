
/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/
//
//  LP_nativeDEX.c
//  marketmaker
//

#include <stdio.h>
#include "LP_include.h"
#include "LP_network.c"

#define LP_PROPAGATION_SLACK 10 // txid ordering is not enforced, so getting extra recent txid

char *activecoins[] = { "BTC", "KMD", };//"LTC", "USD", "REVS", "JUMBLR" };

char *default_LPnodes[] = { "5.9.253.195", "5.9.253.196", "5.9.253.197", "5.9.253.198", "5.9.253.199", "5.9.253.200", "5.9.253.201", "5.9.253.202", "5.9.253.203", "5.9.253.204" };
portable_mutex_t LP_peermutex,LP_utxomutex,LP_commandmutex,LP_cachemutex;
int32_t LP_mypubsock = -1;
int32_t Client_connections;

struct LP_peerinfo
{
    UT_hash_handle hh;
    uint64_t ip_port;
    double profitmargin;
    uint32_t ipbits,errortime,errors,numpeers,numutxos,lasttime,connected;
    int32_t pushsock,subsock;
    uint16_t port;
    char ipaddr[64];
} *LP_peerinfos,*LP_mypeer;

struct LP_utxoinfo
{
    UT_hash_handle hh;
    bits256 txid,deposittxid,feetxid,otherpubkey;
    void *swap;
    uint64_t satoshis,depositsatoshis,feesatoshis;
    uint8_t key[sizeof(bits256) + sizeof(int32_t)];
    int32_t vout,depositvout,feevout,pair; uint32_t lasttime,errors,swappending;
    double profitmargin;
    char ipaddr[64],coinaddr[64],spendscript[256],coin[16];
    uint16_t port;
} *LP_utxoinfos;

struct LP_cacheinfo
{
    UT_hash_handle hh;
    uint8_t key[sizeof(bits256)+sizeof(uint64_t)*2+sizeof(int32_t)];
    double price;
    uint64_t satoshis,destsatoshis;
    uint32_t timestamp;
} *LP_cacheinfos;

int32_t LP_cachekey(uint8_t *key,char *base,char *rel,bits256 txid,int32_t vout)
{
    uint64_t basebits,relbits; int32_t offset = 0;
    basebits = stringbits(base);
    relbits = stringbits(rel);
    memcpy(&key[offset],&basebits,sizeof(basebits)), offset += sizeof(basebits);
    memcpy(&key[offset],&relbits,sizeof(relbits)), offset += sizeof(relbits);
    memcpy(&key[offset],&txid,sizeof(txid)), offset += sizeof(txid);
    memcpy(&key[offset],&vout,sizeof(vout)), offset += sizeof(vout);
    return(offset);
}

struct LP_cacheinfo *LP_cachefind(char *base,char *rel,bits256 txid,int32_t vout)
{
    struct LP_cacheinfo *ptr=0; uint8_t key[sizeof(bits256)+sizeof(uint64_t)*2+sizeof(vout)];
    if ( LP_cachekey(key,base,rel,txid,vout) == sizeof(key) )
    {
        portable_mutex_lock(&LP_cachemutex);
        HASH_FIND(hh,LP_cacheinfos,key,sizeof(key),ptr);
        portable_mutex_unlock(&LP_cachemutex);
    } else printf("LP_cachefind keysize mismatch?\n");
    if ( ptr != 0 && ptr->timestamp != 0 && ptr->timestamp < time(NULL)-LP_CACHEDURATION )
    {
        printf("expire price %.8f\n",ptr->price);
        ptr->price = 0.;
        ptr->destsatoshis = 0;
        ptr->timestamp = 0;
    }
    return(ptr);
}

struct LP_cacheinfo *LP_cacheadd(char *base,char *rel,bits256 txid,int32_t vout,double price,uint64_t satoshis)
{
    struct LP_cacheinfo *ptr=0;
    if ( (ptr= LP_cachefind(base,rel,txid,vout)) == 0 )
    {
        ptr = calloc(1,sizeof(*ptr));
        if ( LP_cachekey(ptr->key,base,rel,txid,vout) == sizeof(ptr->key) )
        {
            portable_mutex_lock(&LP_cachemutex);
            HASH_ADD(hh,LP_cacheinfos,key,sizeof(ptr->key),ptr);
            portable_mutex_unlock(&LP_cachemutex);
        } else printf("LP_cacheadd keysize mismatch?\n");
    }
    ptr->price = price;
    ptr->satoshis = satoshis;
    ptr->destsatoshis = satoshis * price;
    ptr->timestamp = (uint32_t)time(NULL);
    printf("updated %s/%s %llu price %.8f\n",base,rel,(long long)satoshis,price);
    return(ptr);
}

double LP_pricecache(uint64_t *destsatoshisp,char *base,char *rel,bits256 txid,int32_t vout)
{
    struct LP_cacheinfo *ptr;
    if ( (ptr= LP_cachefind(base,rel,txid,vout)) != 0 )
    {
        *destsatoshisp = ptr->destsatoshis;
        return(ptr->price);
    } else *destsatoshisp = 0;
    return(0.);
}

struct LP_peerinfo *LP_peerfind(uint32_t ipbits,uint16_t port)
{
    struct LP_peerinfo *peer=0; uint64_t ip_port;
    ip_port = ((uint64_t)port << 32) | ipbits;
    portable_mutex_lock(&LP_peermutex);
    HASH_FIND(hh,LP_peerinfos,&ip_port,sizeof(ip_port),peer);
    portable_mutex_unlock(&LP_peermutex);
    return(peer);
}

struct LP_utxoinfo *LP_utxofind(bits256 txid,int32_t vout)
{
    struct LP_utxoinfo *utxo=0; uint8_t key[sizeof(txid) + sizeof(vout)];
    memcpy(key,txid.bytes,sizeof(txid));
    memcpy(&key[sizeof(txid)],&vout,sizeof(vout));
    portable_mutex_lock(&LP_utxomutex);
    HASH_FIND(hh,LP_utxoinfos,key,sizeof(key),utxo);
    portable_mutex_unlock(&LP_utxomutex);
    return(utxo);
}

cJSON *LP_peerjson(struct LP_peerinfo *peer)
{
    cJSON *item = cJSON_CreateObject();
    jaddstr(item,"ipaddr",peer->ipaddr);
    jaddnum(item,"port",peer->port);
    jaddnum(item,"profit",peer->profitmargin);
    return(item);
}

cJSON *LP_utxojson(struct LP_utxoinfo *utxo)
{
    cJSON *item = cJSON_CreateObject();
    jaddstr(item,"ipaddr",utxo->ipaddr);
    jaddnum(item,"port",utxo->port);
    jaddnum(item,"profit",utxo->profitmargin);
    jaddstr(item,"base",utxo->coin);
    jaddstr(item,"address",utxo->coinaddr);
    jaddstr(item,"script",utxo->spendscript);
    jaddbits256(item,"txid",utxo->txid);
    jaddnum(item,"vout",utxo->vout);
    jaddnum(item,"value",dstr(utxo->satoshis));
    jaddbits256(item,"deposit",utxo->deposittxid);
    jaddnum(item,"dvout",utxo->depositvout);
    jaddnum(item,"dvalue",dstr(utxo->depositsatoshis));
    return(item);
}

char *LP_peers()
{
    struct LP_peerinfo *peer,*tmp; cJSON *peersjson = cJSON_CreateArray();
    HASH_ITER(hh,LP_peerinfos,peer,tmp)
    {
        jaddi(peersjson,LP_peerjson(peer));
    }
    return(jprint(peersjson,1));
}

char *LP_utxos(struct LP_peerinfo *mypeer,char *coin,int32_t lastn)
{
    int32_t i,firsti; struct LP_utxoinfo *utxo,*tmp; cJSON *utxosjson = cJSON_CreateArray();
    i = 0;
    if ( lastn >= mypeer->numutxos )
        firsti = -1;
    else firsti = (mypeer->numutxos - lastn);
    HASH_ITER(hh,LP_utxoinfos,utxo,tmp)
    {
        if ( i++ < firsti )
            continue;
        if ( coin == 0 || coin[0] == 0 || strcmp(coin,utxo->coin) == 0 )
        {
            jaddi(utxosjson,LP_utxojson(utxo));
        }
    }
    return(jprint(utxosjson,1));
}

struct LP_peerinfo *LP_addpeer(int32_t amclient,struct LP_peerinfo *mypeer,int32_t mypubsock,char *ipaddr,uint16_t port,uint16_t pushport,uint16_t subport,double profitmargin,int32_t numpeers,int32_t numutxos)
{
    uint32_t ipbits; int32_t pushsock,subsock,timeout,enabled; char checkip[64],pushaddr[64],subaddr[64]; struct LP_peerinfo *peer = 0;
    ipbits = (uint32_t)calc_ipbits(ipaddr);
    expand_ipbits(checkip,ipbits);
    if ( strcmp(checkip,ipaddr) == 0 )
    {
        //printf("LPaddpeer %s\n",ipaddr);
        if ( (peer= LP_peerfind(ipbits,port)) != 0 )
        {
            if ( peer->profitmargin == 0. )
                peer->profitmargin = profitmargin;
            if ( numpeers > peer->numpeers )
                peer->numpeers = numpeers;
            if ( numutxos > peer->numutxos )
                peer->numutxos = numutxos;
        }
        else
        {
            peer = calloc(1,sizeof(*peer));
            peer->pushsock = peer->subsock = pushsock = subsock = -1;
            strcpy(peer->ipaddr,ipaddr);
            if ( amclient == 0 )
                enabled = 1;
            else enabled = (rand() % (1 << Client_connections)) == 0;
            if ( enabled != 0 && pushport != 0 && subport != 0 && (pushsock= nn_socket(AF_SP,NN_PUSH)) >= 0 )
            {
                timeout = 1000;
                nn_setsockopt(pushsock,NN_SOL_SOCKET,NN_SNDTIMEO,&timeout,sizeof(timeout));
                nanomsg_tcpname(pushaddr,peer->ipaddr,pushport);
                if ( nn_connect(peer->pushsock,pushaddr) >= 0 )
                {
                    printf("connected to push.(%s) %d\n",pushaddr,peer->pushsock);
                    peer->connected = (uint32_t)time(NULL);
                    peer->pushsock = pushsock;
                    if ( enabled != 0 && (subsock= nn_socket(AF_SP,NN_SUB)) >= 0 )
                    {
                        timeout = 1;
                        nn_setsockopt(subsock,NN_SOL_SOCKET,NN_RCVTIMEO,&timeout,sizeof(timeout));
                        nn_setsockopt(subsock,NN_SUB,NN_SUB_SUBSCRIBE,"",0);
                        nanomsg_tcpname(subaddr,peer->ipaddr,subport);
                        if ( nn_connect(peer->subsock,subaddr) >= 0 )
                        {
                            peer->subsock = subsock;
                            printf("connected to sub.(%s) %d\n",subaddr,peer->subsock);
                            Client_connections += amclient;
                        } else nn_close(subsock);
                    }
                } else nn_close(pushsock);
            }
            peer->profitmargin = profitmargin;
            peer->ipbits = ipbits;
            peer->port = port;
            peer->ip_port = ((uint64_t)port << 32) | ipbits;
            portable_mutex_lock(&LP_peermutex);
            HASH_ADD(hh,LP_peerinfos,ip_port,sizeof(peer->ip_port),peer);
            if ( mypeer != 0 )
            {
                mypeer->numpeers++;
                printf("_LPaddpeer %s -> numpeers.%d mypubsock.%d other.(%d %d)\n",ipaddr,mypeer->numpeers,mypubsock,numpeers,numutxos);
            } else peer->numpeers = 1; // will become mypeer
            portable_mutex_unlock(&LP_peermutex);
            if ( mypubsock >= 0 )
                LP_send(mypubsock,jprint(LP_peerjson(peer),1),1);
        }
    }
    return(peer);
}

struct LP_utxoinfo *LP_addutxo(int32_t amclient,struct LP_peerinfo *mypeer,int32_t mypubsock,char *coin,bits256 txid,int32_t vout,int64_t satoshis,bits256 deposittxid,int32_t depositvout,int64_t depositsatoshis,char *spendscript,char *coinaddr,char *ipaddr,uint16_t port,double profitmargin)
{
    struct LP_utxoinfo *utxo = 0; uint8_t key[sizeof(txid) + sizeof(vout)];
    if ( coin == 0 || coin[0] == 0 || spendscript == 0 || spendscript[0] == 0 || coinaddr == 0 || coinaddr[0] == 0 || bits256_nonz(txid) == 0 || bits256_nonz(deposittxid) == 0 || vout < 0 || depositvout < 0 || satoshis <= 0 || depositsatoshis <= 0 )
    {
        printf("malformed addutxo %d %d %d %d %d %d %d %d %d\n", coin == 0,spendscript == 0,coinaddr == 0,bits256_nonz(txid) == 0,bits256_nonz(deposittxid) == 0,vout < 0,depositvout < 0,satoshis <= 0,depositsatoshis <= 0);
        return(0);
    }
    if ( (utxo= LP_utxofind(txid,vout)) != 0 )
    {
        if ( bits256_cmp(txid,utxo->txid) != 0 || bits256_cmp(deposittxid,utxo->deposittxid) != 0 || vout != utxo->vout || satoshis != utxo->satoshis || depositvout != utxo->depositvout || depositsatoshis != utxo->depositsatoshis || strcmp(coin,utxo->coin) != 0 || strcmp(spendscript,utxo->spendscript) != 0 || strcmp(coinaddr,utxo->coinaddr) != 0 || strcmp(ipaddr,utxo->ipaddr) != 0 || port != utxo->port )
        {
            utxo->errors++;
            char str[65]; printf("error on subsequent utxo add.(%s)\n",bits256_str(str,txid));
        }
        else if ( profitmargin != 0. )
            utxo->profitmargin = profitmargin;
    }
    else
    {
        utxo = calloc(1,sizeof(*utxo));
        utxo->pair = -1;
        utxo->profitmargin = profitmargin;
        strcpy(utxo->ipaddr,ipaddr);
        utxo->port = port;
        safecopy(utxo->coin,coin,sizeof(utxo->coin));
        safecopy(utxo->coinaddr,coinaddr,sizeof(utxo->coinaddr));
        safecopy(utxo->spendscript,spendscript,sizeof(utxo->spendscript));
        utxo->txid = txid;
        utxo->vout = vout;
        utxo->satoshis = satoshis;
        if ( amclient == 0 )
        {
            utxo->deposittxid = deposittxid;
            utxo->depositvout = depositvout;
            utxo->depositsatoshis = depositsatoshis;
        }
        else
        {
            utxo->feetxid = deposittxid;
            utxo->feevout = depositvout;
            utxo->feesatoshis = depositsatoshis;
        }
        memcpy(key,txid.bytes,sizeof(txid));
        memcpy(&key[sizeof(txid)],&vout,sizeof(vout));
        memcpy(utxo->key,key,sizeof(key));
        portable_mutex_lock(&LP_utxomutex);
        HASH_ADD(hh,LP_utxoinfos,key,sizeof(key),utxo);
        if ( mypeer != 0 )
            mypeer->numutxos++;
        portable_mutex_unlock(&LP_utxomutex);
        if ( mypubsock >= 0 )
            LP_send(mypubsock,jprint(LP_utxojson(utxo),1),1);
        printf("%s:%u LP_addutxo.(%.8f %.8f) numutxos.%d\n",ipaddr,port,dstr(satoshis),dstr(depositsatoshis),mypeer!=0?mypeer->numutxos:0);
    }
    return(utxo);
}

int32_t LP_peersparse(int32_t amclient,struct LP_peerinfo *mypeer,int32_t mypubsock,char *destipaddr,uint16_t destport,char *retstr,uint32_t now)
{
    struct LP_peerinfo *peer; uint32_t argipbits; char *argipaddr; uint16_t argport,pushport,subport; cJSON *array,*item; int32_t i,n=0;
    if ( (array= cJSON_Parse(retstr)) != 0 )
    {
        if ( (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (argipaddr= jstr(item,"ipaddr")) != 0 && (argport= juint(item,"port")) != 0 )
                {
                    if ( (pushport= juint(item,"push")) == 0 )
                        pushport = argport + 1;
                    if ( (subport= juint(item,"sub")) == 0 )
                        subport = argport + 2;
                    argipbits = (uint32_t)calc_ipbits(argipaddr);
                    if ( (peer= LP_peerfind(argipbits,argport)) == 0 )
                        peer = LP_addpeer(amclient,mypeer,mypubsock,argipaddr,argport,pushport,subport,jdouble(item,"profit"),jint(item,"numpeers"),jint(item,"numutxos"));
                    if ( peer != 0 )
                    {
                        peer->lasttime = now;
                        if ( strcmp(argipaddr,destipaddr) == 0 && destport == argport && peer->numpeers != n )
                            peer->numpeers = n;
                    }
                }
            }
        }
        free_json(array);
    }
    return(n);
}

int32_t LP_utxosparse(int32_t amclient,struct LP_peerinfo *mypeer,int32_t mypubsock,char *destipaddr,uint16_t destport,char *retstr,uint32_t now)
{
    struct LP_peerinfo *peer,*destpeer; uint32_t argipbits; char *argipaddr; uint16_t argport,pushport,subport; cJSON *array,*item; int32_t i,n=0; bits256 txid; struct LP_utxoinfo *utxo;
    if ( amclient != 0 )
    {
        printf("LP_utxosparse not for clientside\n");
        return(-1);
    }
    if ( (array= cJSON_Parse(retstr)) != 0 )
    {
        if ( (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (argipaddr= jstr(item,"ipaddr")) != 0 && (argport= juint(item,"port")) != 0 )
                {
                    if ( (pushport= juint(item,"push")) == 0 )
                        pushport = argport + 1;
                    if ( (subport= juint(item,"sub")) == 0 )
                        subport = argport + 2;
                    argipbits = (uint32_t)calc_ipbits(argipaddr);
                    if ( (peer= LP_peerfind(argipbits,argport)) == 0 )
                        peer = LP_addpeer(amclient,mypeer,mypubsock,argipaddr,argport,pushport,subport,jdouble(item,"profit"),jint(item,"numpeers"),jint(item,"numutxos"));
                    if ( jobj(item,"txid") != 0 )
                    {
                        txid = jbits256(item,"txid");
                        utxo = LP_addutxo(amclient,mypeer,mypubsock,jstr(item,"base"),txid,jint(item,"vout"),SATOSHIDEN*jdouble(item,"value"),jbits256(item,"deposit"),jint(item,"dvout"),SATOSHIDEN * jdouble(item,"dvalue"),jstr(item,"script"),jstr(item,"address"),argipaddr,argport,jdouble(item,"profit"));
                        if ( utxo != 0 )
                            utxo->lasttime = now;
                    }
                }
            }
            if ( (destpeer= LP_peerfind((uint32_t)calc_ipbits(destipaddr),destport)) != 0 )
            {
                if ( destpeer->numutxos < n )
                {
                    destpeer->numutxos = n;
                    printf("got.(%s) from %s numutxos.%d\n",retstr,destpeer->ipaddr,destpeer->numutxos);
                }
            }
        }
        free_json(array);
    }
    return(n);
}

char *issue_LP_getpeers(char *destip,uint16_t destport,char *ipaddr,uint16_t port,double profitmargin,int32_t numpeers,int32_t numutxos)
{
    char url[512],*retstr;
    sprintf(url,"http://%s:%u/api/stats/getpeers?ipaddr=%s&port=%u&profit=%.6f&numpeers=%d&numutxos=%d",destip,destport,ipaddr,port,profitmargin,numpeers,numutxos);
    //printf("send.(%s)\n",url);
    retstr = issue_curl(url);
    //printf("GETPEERS.(%s)\n",retstr);
    return(retstr);
}

char *issue_LP_getutxos(char *destip,uint16_t destport,char *coin,int32_t lastn,char *ipaddr,uint16_t port,double profitmargin,int32_t numpeers,int32_t numutxos)
{
    char url[512];
    sprintf(url,"http://%s:%u/api/stats/getutxos?coin=%s&lastn=%d&ipaddr=%s&port=%u&profit=%.6f&numpeers=%d&numutxos=%d",destip,destport,coin,lastn,ipaddr,port,profitmargin,numpeers,numutxos);
    return(issue_curl(url));
}

char *issue_LP_clientgetutxos(char *destip,uint16_t destport,char *coin,int32_t lastn)
{
    char url[512];
    sprintf(url,"http://%s:%u/api/stats/getutxos?coin=%s&lastn=%d&ipaddr=127.0.0.1&port=0",destip,destport,coin,lastn);
    return(issue_curl(url));
}

char *issue_LP_notify(char *destip,uint16_t destport,char *ipaddr,uint16_t port,double profitmargin,int32_t numpeers,int32_t numutxos)
{
    char url[512];
    sprintf(url,"http://%s:%u/api/stats/notify?ipaddr=%s&port=%u&profit=%.6f&numpeers=%d&numutxos=%d",destip,destport,ipaddr,port,profitmargin,numpeers,numutxos);
    return(issue_curl(url));
}

char *issue_LP_notifyutxo(char *destip,uint16_t destport,struct LP_utxoinfo *utxo)
{
    char url[4096],str[65],str2[65];
    sprintf(url,"http://%s:%u/api/stats/notifyutxo?ipaddr=%s&port=%u&profit=%.6f&coin=%s&txid=%s&vout=%d&value=%.8f&deposit=%s&dvout=%d&dvalue=%.8f&script=%s&address=%s",destip,destport,utxo->ipaddr,utxo->port,utxo->profitmargin,utxo->coin,bits256_str(str,utxo->txid),utxo->vout,dstr(utxo->satoshis),bits256_str(str2,utxo->deposittxid),utxo->depositvout,dstr(utxo->depositsatoshis),utxo->spendscript,utxo->coinaddr);
    if ( strlen(url) > 1024 )
        printf("WARNING long url.(%s)\n",url);
    return(issue_curl(url));
}

void LP_peersquery(int32_t amclient,struct LP_peerinfo *mypeer,int32_t mypubsock,char *destipaddr,uint16_t destport,char *myipaddr,uint16_t myport,double myprofit)
{
    char *retstr; struct LP_peerinfo *peer,*tmp; uint32_t now,flag = 0;
    peer = LP_peerfind((uint32_t)calc_ipbits(destipaddr),destport);
    if ( peer != 0 && peer->errors > 0 )
        return;
    if ( (retstr= issue_LP_getpeers(destipaddr,destport,myipaddr,myport,myprofit,mypeer!=0?mypeer->numpeers:0,mypeer!=0?mypeer->numutxos:0)) != 0 )
    {
        //printf("got.(%s)\n",retstr);
        now = (uint32_t)time(NULL);
        LP_peersparse(amclient,mypeer,mypubsock,destipaddr,destport,retstr,now);
        free(retstr);
        if ( mypeer != 0 )
        {
            HASH_ITER(hh,LP_peerinfos,peer,tmp)
            {
                if ( peer->lasttime != now )
                {
                    printf("{%s:%u %.6f}.%d ",peer->ipaddr,peer->port,peer->profitmargin,peer->lasttime - now);
                    flag++;
                    if ( (retstr= issue_LP_notify(destipaddr,destport,peer->ipaddr,peer->port,peer->profitmargin,peer->numpeers,0)) != 0 )
                        free(retstr);
                }
            }
            if ( flag != 0 )
                printf(" <- missing peers\n");
        }
    } else if ( peer != 0 )
        peer->errors++;
}

void LP_utxosquery(int32_t amclient,struct LP_peerinfo *mypeer,int32_t mypubsock,char *destipaddr,uint16_t destport,char *coin,int32_t lastn,char *myipaddr,uint16_t myport,double myprofit)
{
    char *retstr; struct LP_utxoinfo *utxo,*tmp; struct LP_peerinfo *peer; int32_t i,firsti; uint32_t now,flag = 0;
    if ( amclient != 0 )
    {
        printf("LP_utxosquery not for clientside\n");
        return;
    }
    peer = LP_peerfind((uint32_t)calc_ipbits(destipaddr),destport);
    if ( (peer != 0 && peer->errors > 0) || mypeer == 0 )
        return;
    if ( coin == 0 )
        coin = "";
    if ( (retstr= issue_LP_getutxos(destipaddr,destport,coin,lastn,myipaddr,myport,myprofit,mypeer->numpeers,mypeer->numutxos)) != 0 )
    {
        now = (uint32_t)time(NULL);
        LP_utxosparse(amclient,mypeer,mypubsock,destipaddr,destport,retstr,now);
        free(retstr);
        i = 0;
        if ( lastn >= mypeer->numutxos )
            firsti = -1;
        else firsti = (mypeer->numutxos - lastn);
        HASH_ITER(hh,LP_utxoinfos,utxo,tmp)
        {
            if ( i++ < firsti )
                continue;
            if ( utxo->lasttime != now )
            {
                char str[65]; printf("{%s:%u %s} ",utxo->ipaddr,utxo->port,bits256_str(str,utxo->txid));
                flag++;
                if ( (retstr= issue_LP_notifyutxo(destipaddr,destport,utxo)) != 0 )
                    free(retstr);
            }
        }
        if ( flag != 0 )
            printf(" <- missing utxos\n");
    } else if ( peer != 0 )
        peer->errors++;
}

int32_t LP_maxvalue(uint64_t *values,int32_t n)
{
    int32_t i,maxi = -1; uint64_t maxval = 0;
    for (i=0; i<n; i++)
        if ( values[i] > maxval )
        {
            maxi = i;
            maxval = values[i];
        }
    return(maxi);
}

int32_t LP_nearestvalue(uint64_t *values,int32_t n,uint64_t targetval)
{
    int32_t i,mini = -1; int64_t dist; uint64_t mindist = (1 << 31);
    for (i=0; i<n; i++)
    {
        dist = (values[i] - targetval);
        if ( dist < 0 && -dist < values[i]/10 )
            dist = -dist;
        if ( dist >= 0 && dist < mindist )
        {
            mini = i;
            mindist = dist;
        }
    }
    return(mini);
}

int32_t basilisk_istrustedbob(struct basilisk_swap *swap)
{
    // for BTC and if trusted LP
    return(0);
}

void tradebot_swap_balancingtrade(struct basilisk_swap *swap,int32_t iambob)
{
    
}

void tradebot_pendingadd(cJSON *tradejson,char *base,double basevolume,char *rel,double relvolume)
{
    // add to trades
}

char GLOBAL_DBDIR[] = ".";

#include "LP_secp.c"
#include "LP_rpc.c"
#include "LP_bitcoin.c"
#include "LP_transaction.c"
#include "LP_remember.c"
#include "LP_statemachine.c"
#include "LP_swap.c"
#include "LP_commands.c"

char *parse_conf_line(char *line,char *field)
{
    line += strlen(field);
    for (; *line!='='&&*line!=0; line++)
        break;
    if ( *line == 0 )
        return(0);
    if ( *line == '=' )
        line++;
    while ( line[strlen(line)-1] == '\r' || line[strlen(line)-1] == '\n' || line[strlen(line)-1] == ' ' )
        line[strlen(line)-1] = 0;
    //printf("LINE.(%s)\n",line);
    _stripwhite(line,0);
    return(clonestr(line));
}

void LP_userpassfp(char *username,char *password,FILE *fp)
{
    char *rpcuser,*rpcpassword,*str,line[8192];
    rpcuser = rpcpassword = 0;
    username[0] = password[0] = 0;
    while ( fgets(line,sizeof(line),fp) != 0 )
    {
        if ( line[0] == '#' )
            continue;
        //printf("line.(%s) %p %p\n",line,strstr(line,(char *)"rpcuser"),strstr(line,(char *)"rpcpassword"));
        if ( (str= strstr(line,(char *)"rpcuser")) != 0 )
            rpcuser = parse_conf_line(str,(char *)"rpcuser");
        else if ( (str= strstr(line,(char *)"rpcpassword")) != 0 )
            rpcpassword = parse_conf_line(str,(char *)"rpcpassword");
    }
    if ( rpcuser != 0 && rpcpassword != 0 )
    {
        strcpy(username,rpcuser);
        strcpy(password,rpcpassword);
    }
    //printf("rpcuser.(%s) rpcpassword.(%s) KMDUSERPASS.(%s) %u\n",rpcuser,rpcpassword,KMDUSERPASS,port);
    if ( rpcuser != 0 )
        free(rpcuser);
    if ( rpcpassword != 0 )
        free(rpcpassword);
}

void LP_statefname(char *fname,char *symbol,char *assetname,char *str)
{
    sprintf(fname,"%s",LP_getdatadir());
#ifdef WIN32
    strcat(fname,"\\");
#else
    strcat(fname,"/");
#endif
    if ( strcmp(symbol,"BTC") == 0 )
        strcat(fname,".bitcoin");
    else if ( strcmp(symbol,"LTC") == 0 )
        strcat(fname,".litecoin");
    else
    {
        strcat(fname,".komodo");
        if ( strcmp(symbol,"KMD") != 0 )
        {
#ifdef WIN32
            strcat(fname,"\\");
#else
            strcat(fname,"/");
#endif
            strcat(fname,assetname);
        }
    }
#ifdef WIN32
    strcat(fname,"\\");
#else
    strcat(fname,"/");
#endif
    strcat(fname,str);
    printf("LP_statefname.(%s) <- %s %s %s\n",fname,symbol,assetname,str);
}

int32_t LP_userpass(char *userpass,char *symbol,char *assetname,char *confroot)
{
    FILE *fp; char fname[512],username[512],password[512],confname[16];
    userpass[0] = 0;
    sprintf(confname,"%s.conf",confroot);
#ifdef __APPLE__
    confname[0] = toupper(confname[0]);
#endif
    LP_statefname(fname,symbol,assetname,confname);
    if ( (fp= fopen(fname,"rb")) != 0 )
    {
        LP_userpassfp(username,password,fp);
        sprintf(userpass,"%s:%s",username,password);
        fclose(fp);
        return((int32_t)strlen(userpass));
    }
    return(-1);
}

uint32_t LP_assetmagic(char *symbol,uint64_t supply)
{
    uint8_t buf[512]; int32_t len = 0;
    if ( strcmp(symbol,"KMD") == 0 )
        return(0x8de4eef9);
    len = iguana_rwnum(1,&buf[len],sizeof(supply),(void *)&supply);
    strcpy((char *)&buf[len],symbol);
    len += strlen(symbol);
    return(calc_crc32(0,buf,len));
}

uint16_t LP_assetport(uint32_t magic)
{
    if ( magic == 0x8de4eef9 )
        return(7771);
    else return(8000 + (magic % 7777));
}

uint16_t LP_rpcport(char *symbol,uint64_t supply,uint32_t *magicp)
{
    if ( symbol == 0 || symbol[0] == 0 || strcmp("KMD",symbol) == 0 )
    {
        *magicp = 0x8de4eef9;
        return(7771);
    }
    else if ( strcmp("BTC",symbol) == 0 )
        return(8332);
    else if ( strcmp("LTC",symbol) == 0 )
        return(9332);
    *magicp = LP_assetmagic(symbol,supply);
    return(LP_assetport(*magicp)+1);
}

struct iguana_info *LP_coinfind(char *symbol)
{
    static struct iguana_info *LP_coins; static int32_t LP_numcoins;
    struct iguana_info *coin,cdata; int32_t i; uint32_t magic; uint16_t port;
    for (i=0; i<LP_numcoins; i++)
        if ( strcmp(LP_coins[i].symbol,symbol) == 0 )
            return(&LP_coins[i]);
    memset(&cdata,0,sizeof(cdata));
    coin = &cdata;
    safecopy(cdata.symbol,symbol,sizeof(cdata.symbol));
    port = LP_rpcport(symbol,10,&magic);
    sprintf(cdata.serverport,"127.0.0.1:%u",port);
    cdata.longestchain = 100000;
    cdata.txfee = 10000;
    cdata.estimatedrate = 20;
    if ( strcmp(symbol,"BTC") == 0 )
    {
        cdata.txfee = 50000;
        cdata.estimatedrate = 300;
        cdata.p2shtype = 5;
        cdata.wiftype = 128;
        LP_userpass(cdata.userpass,symbol,"","bitcoin");
    }
    else if ( strcmp(symbol,"LTC") == 0 )
    {
        cdata.pubtype = 48;
        cdata.p2shtype = 5;
        cdata.wiftype = 176;
        LP_userpass(cdata.userpass,symbol,"","litecoin");
    }
    else
    {
        cdata.isPoS = 1;
        cdata.pubtype = 60;
        cdata.p2shtype = 85;
        cdata.wiftype = 188;
        LP_userpass(cdata.userpass,symbol,symbol,strcmp(symbol,"KMD") == 0 ? "komodo" : symbol);
    }
    //printf("%s: (%s) (%s)\n",symbol,cdata.serverport,cdata.userpass);
    LP_coins = realloc(LP_coins,sizeof(*LP_coins) * (LP_numcoins+1));
    coin = &LP_coins[LP_numcoins++];
    *coin = cdata;
    return(coin);
}

uint64_t LP_privkey_init(struct LP_peerinfo *mypeer,int32_t mypubsock,char *symbol,char *passphrase,char *wifstr,int32_t amclient)
{
    char coinaddr[64],*script; cJSON *array,*item,*retjson; bits256 txid,deposittxid; int32_t used,i,n,vout,depositvout; uint64_t *values,satoshis,depositval,targetval,value,total = 0; bits256 privkey,pubkey; uint8_t pubkey33[33],tmptype,rmd160[20]; struct iguana_info *coin = LP_coinfind(symbol);
    if ( coin == 0 )
    {
        printf("cant add privkey for %s, coin not active\n",symbol);
        return(0);
    }
    if ( passphrase != 0 )
        conv_NXTpassword(privkey.bytes,pubkey.bytes,(uint8_t *)passphrase,(int32_t)strlen(passphrase));
    else privkey = iguana_wif2privkey(wifstr);
    iguana_priv2pub(pubkey33,coinaddr,privkey,coin->pubtype);
    {
        char tmpstr[128];
        bitcoin_priv2wif(tmpstr,privkey,coin->wiftype);
        printf("%s coinaddr.(%s) %d wif.(%s) passphrase.(%s)\n",symbol,coinaddr,coin->pubtype,tmpstr,passphrase);
        if ( (retjson= LP_importprivkey(coin->symbol,tmpstr,coinaddr,-1)) != 0 )
            printf("importprivkey -> (%s)\n",jprint(retjson,1));
    }
    bitcoin_addr2rmd160(&tmptype,rmd160,coinaddr);
    LP_privkeyadd(privkey,rmd160);
    if ( (array= LP_listunspent(symbol,coinaddr)) != 0 )
    {
        if ( is_cJSON_Array(array) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
        {
            values = calloc(n,sizeof(*values));
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                satoshis = SATOSHIDEN * jdouble(item,"amount");
                values[i] = satoshis;
                printf("%.8f ",dstr(satoshis));
            }
            printf("array.%d\n",n);
            used = 0;
            while ( used < n )
            {
                if ( (i= LP_maxvalue(values,n)) >= 0 )
                {
                    item = jitem(array,i);
                    deposittxid = jbits256(item,"txid");
                    depositvout = juint(item,"vout");
                    script = jstr(item,"scriptPubKey");
                    depositval = values[i];
                    values[i] = 0, used++;
                    if ( amclient != 0 )
                        targetval = (depositval / 777);
                    else targetval = (depositval / 9) * 8;
                    printf("i.%d %.8f target %.8f\n",i,dstr(depositval),dstr(targetval));
                    if ( (i= LP_nearestvalue(values,n,targetval)) >= 0 )
                    {
                        item = jitem(array,i);
                        txid = jbits256(item,"txid");
                        vout = juint(item,"vout");
                        if ( jstr(item,"scriptPubKey") != 0 && strcmp(script,jstr(item,"scriptPubKey")) == 0 )
                        {
                            value = values[i];
                            values[i] = 0, used++;
                            if ( amclient == 0 )
                                LP_addutxo(amclient,mypeer,mypubsock,symbol,txid,vout,value,deposittxid,depositvout,depositval,script,coinaddr,LP_peerinfos[0].ipaddr,LP_peerinfos[0].port,LP_peerinfos[0].profitmargin);
                            else LP_addutxo(amclient,mypeer,mypubsock,symbol,txid,vout,value,deposittxid,depositvout,depositval,script,coinaddr,"127.0.0.1",0,0);
                            total += value;
                        }
                    }
                } else break;
            }
            free(values);
        }
        free_json(array);
    }
    return(total);
}

void LP_privkey_updates(struct LP_peerinfo *mypeer,int32_t pubsock,char *passphrase,int32_t amclient)
{
    int32_t i;
    for (i=0; i<sizeof(activecoins)/sizeof(*activecoins); i++)
        LP_privkey_init(mypeer,pubsock,activecoins[i],passphrase,"",amclient);
}

void LP_mainloop(struct LP_peerinfo *mypeer,uint16_t mypubport,int32_t pubsock,int32_t pullsock,uint16_t myport,int32_t amclient,char *passphrase,double profitmargin)
{
    char *retstr; uint8_t r; int32_t i,n,j,len,recvsize,counter=0,nonz,lastn; struct LP_peerinfo *peer,*tmp; void *ptr; cJSON *argjson;
    if ( amclient == 0 )
    {
        for (i=0; i<sizeof(default_LPnodes)/sizeof(*default_LPnodes); i++)
        {
            if ( (rand() % 100) > 25 )
                continue;
            LP_peersquery(amclient,mypeer,pubsock,default_LPnodes[i],myport,mypeer!=0?mypeer->ipaddr:"127.0.0.1",myport,profitmargin);
        }
    }
    else
    {
        OS_randombytes((void *)&r,sizeof(r));
        for (j=0; j<sizeof(default_LPnodes)/sizeof(*default_LPnodes); j++)
        {
            i = (r + j) % (sizeof(default_LPnodes)/sizeof(*default_LPnodes));
            LP_peersquery(amclient,mypeer,pubsock,default_LPnodes[i],myport,mypeer!=0?mypeer->ipaddr:"127.0.0.1",myport,profitmargin);
        }
    }
    if ( OS_thread_create(malloc(sizeof(pthread_t)),NULL,(void *)stats_rpcloop,(void *)&myport) != 0 )
    {
        printf("error launching stats rpcloop for port.%u\n",myport);
        exit(-1);
    }
    for (i=0; i<sizeof(activecoins)/sizeof(*activecoins); i++)
        LP_coinfind(activecoins[i]);
    if ( amclient != 0 )
    {
        while ( 1 )
        {
            nonz = n = 0;
            if ( (counter++ % 60) == 0 )
                LP_privkey_updates(mypeer,pubsock,passphrase,amclient);
            HASH_ITER(hh,LP_peerinfos,peer,tmp)
            {
                n++;
                while ( peer->subsock >= 0 && (recvsize= nn_recv(peer->subsock,&ptr,NN_MSG,0)) >= 0 )
                {
                    nonz++;
                    if ( (argjson= cJSON_Parse((char *)ptr)) != 0 )
                    {
                        portable_mutex_lock(&LP_commandmutex);
                        if ( (retstr= stats_JSON(argjson,"127.0.0.1",mypubport)) != 0 )
                        {
                            printf("%s RECV.[%d] %s\n",peer->ipaddr,recvsize,(char *)ptr);
                            free(retstr);
                        }
                        portable_mutex_unlock(&LP_commandmutex);
                        free_json(argjson);
                    } else printf("error parsing.(%s)\n",(char *)ptr);
                    if ( ptr != 0 )
                        nn_freemsg(ptr), ptr = 0;
                }
            }
            if ( nonz == 0 )
                sleep(n + 1);
        }
    }
    else
    {
        while ( 1 )
        {
            nonz = 0;
            if ( (counter++ % 60) == 0 )
                LP_privkey_updates(mypeer,pubsock,passphrase,amclient);
            HASH_ITER(hh,LP_peerinfos,peer,tmp)
            {
                if ( peer->numpeers != mypeer->numpeers || (rand() % 10) == 0 )
                {
                    if ( peer->numpeers != mypeer->numpeers )
                        printf("%s num.%d vs %d\n",peer->ipaddr,peer->numpeers,mypeer->numpeers);
                    if ( strcmp(peer->ipaddr,mypeer->ipaddr) != 0 )
                        LP_peersquery(amclient,mypeer,pubsock,peer->ipaddr,peer->port,mypeer->ipaddr,myport,profitmargin);
                }
                if ( peer->numutxos != mypeer->numutxos )
                {
                    lastn = peer->numutxos - mypeer->numutxos + LP_PROPAGATION_SLACK;
                    if ( lastn < 0 )
                        lastn = LP_PROPAGATION_SLACK * 2;
                    printf("%s numutxos.%d vs %d lastn.%d\n",peer->ipaddr,peer->numutxos,mypeer->numutxos,lastn);
                    if ( strcmp(peer->ipaddr,mypeer->ipaddr) != 0 )
                        LP_utxosquery(0,mypeer,pubsock,peer->ipaddr,peer->port,"",lastn,mypeer->ipaddr,myport,profitmargin);
                }
                while ( peer->subsock >= 0 && (recvsize= nn_recv(peer->subsock,&ptr,NN_MSG,0)) >= 0 )
                {
                    nonz++;
                    if ( (argjson= cJSON_Parse((char *)ptr)) != 0 )
                    {
                        portable_mutex_lock(&LP_commandmutex);
                        if ( (retstr= stats_JSON(argjson,"127.0.0.1",mypubport)) != 0 )
                        {
                            printf("%s RECV.[%d] %s\n",peer->ipaddr,recvsize,(char *)ptr);
                            free(retstr);
                        }
                        portable_mutex_unlock(&LP_commandmutex);
                        free_json(argjson);
                    } else printf("error parsing.(%s)\n",(char *)ptr);
                    if ( ptr != 0 )
                        nn_freemsg(ptr), ptr = 0;
                }
            }
            while ( pullsock >= 0 && (recvsize= nn_recv(pullsock,&ptr,NN_MSG,0)) >= 0 )
            {
                nonz++;
                if ( (argjson= cJSON_Parse((char *)ptr)) != 0 )
                {
                    len = (int32_t)strlen((char *)ptr) + 1;
                    portable_mutex_lock(&LP_commandmutex);
                    LP_command(mypeer,pubsock,argjson,&((uint8_t *)ptr)[len],recvsize - len,profitmargin);
                    portable_mutex_unlock(&LP_commandmutex);
                    free_json(argjson);
                }
                if ( ptr != 0 )
                    nn_freemsg(ptr), ptr = 0;
            }
            if ( nonz == 0 )
                sleep(mypeer->numpeers + 1);
        }
    }
}

void LPinit(uint16_t myport,uint16_t mypullport,uint16_t mypubport,double profitmargin,char *passphrase,int32_t amclient)
{
    char *myipaddr=0; long filesize,n; int32_t timeout,maxsize,pullsock=-1,pubsock=-1; struct LP_peerinfo *mypeer=0; char pushaddr[128],subaddr[128];
    OS_randombytes((void *)&n,sizeof(n));
    srand((int32_t)n);
    portable_mutex_init(&LP_peermutex);
    portable_mutex_init(&LP_utxomutex);
    portable_mutex_init(&LP_commandmutex);
    portable_mutex_init(&LP_cachemutex);
    if ( amclient == 0 )
    {
        if ( profitmargin == 0. || profitmargin == 0.01 )
        {
            profitmargin = 0.01 + (double)(rand() % 100)/100000;
            printf("default profit margin %f\n",profitmargin);
        }
        if ( system("curl -s4 checkip.amazonaws.com > /tmp/myipaddr") == 0 )
        {
            if ( (myipaddr= OS_filestr(&filesize,"/tmp/myipaddr")) != 0 && myipaddr[0] != 0 )
            {
                n = strlen(myipaddr);
                if ( myipaddr[n-1] == '\n' )
                    myipaddr[--n] = 0;
                pullsock = pubsock = -1;
                nanomsg_tcpname(pushaddr,myipaddr,mypullport);
                nanomsg_tcpname(subaddr,myipaddr,mypubport);
                printf(">>>>>>>>> myipaddr.%s (%s %s)\n",myipaddr,pushaddr,subaddr);
                if ( (pullsock= nn_socket(AF_SP,NN_PULL)) >= 0 && (pubsock= nn_socket(AF_SP,NN_PUB)) >= 0 )
                {
                    if ( nn_bind(pullsock,pushaddr) >= 0 && nn_bind(pubsock,subaddr) >= 0 )
                    {
                        timeout = 10;
                        nn_setsockopt(pubsock,NN_SOL_SOCKET,NN_SNDTIMEO,&timeout,sizeof(timeout));
                        timeout = 1;
                        nn_setsockopt(pullsock,NN_SOL_SOCKET,NN_RCVTIMEO,&timeout,sizeof(timeout));
                        timeout = 1;
                        maxsize = 1024 * 1024;
                        nn_setsockopt(pullsock,NN_SOL_SOCKET,NN_RCVBUF,&maxsize,sizeof(maxsize));
                    }
                    else
                    {
                        printf("error binding to (%s).%d (%s).%d\n",pushaddr,pullsock,subaddr,pubsock);
                        if ( pullsock >= 0 )
                            nn_close(pullsock), pullsock = -1;
                        if ( pubsock >= 0 )
                            nn_close(pubsock), pubsock = -1;
                    }
                } else printf("error getting sockets %d %d\n",pullsock,pubsock);
                LP_mypubsock = pubsock;
                LP_mypeer = mypeer = LP_addpeer(amclient,mypeer,pubsock,myipaddr,myport,0,0,profitmargin,0,0);
                //printf("my ipaddr.(%s) peers.(%s)\n",ipaddr,retstr!=0?retstr:"");
            } else printf("error getting myipaddr\n");
        } else printf("error issuing curl\n");
        if ( myipaddr == 0 || mypeer == 0 )
        {
            printf("couldnt get myipaddr or null mypeer.%p\n",mypeer);
            exit(-1);
        }
        printf("utxos.(%s)\n",LP_utxos(mypeer,"",10000));
    }
    LP_mainloop(mypeer,mypubport,pubsock,pullsock,myport,amclient,passphrase,profitmargin);
}

/*#ifdef __APPLE__
int32_t nn_bind() { return(-1); }
int32_t nn_close() { return(-1); }
int32_t nn_connect() { return(-1); }
int32_t nn_freemsg() { return(-1); }
int32_t nn_poll() { return(-1); }
int32_t nn_recv() { return(-1); }
int32_t nn_send() { return(-1); }
int32_t nn_setsockopt() { return(-1); }
int32_t nn_socket() { return(-1); }

#endif*/
