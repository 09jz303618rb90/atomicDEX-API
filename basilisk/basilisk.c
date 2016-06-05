/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

#include "../iguana/iguana777.h"

typedef char *basilisk_coinfunc(struct supernet_info *myinfo,struct iguana_info *coin,struct iguana_peer *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen);
typedef char *basilisk_servicefunc(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 pubkey,int32_t from_basilisk);
typedef struct basilisk_item *basilisk_requestfunc(struct basilisk_item *Lptr,struct supernet_info *myinfo,bits256 pubkey,cJSON *valsobj,uint8_t *data,int32_t datalen);

uint32_t basilisk_calcnonce(struct supernet_info *myinfo,uint8_t *data,int32_t datalen,uint32_t nBits)
{
    int32_t i,numiters = 0; bits256 hash,hash2,threshold; uint32_t basilisktag;
    vcalc_sha256(0,hash.bytes,data,datalen);
    threshold = bits256_from_compact(nBits);
    for (i=0; i<numiters; i++)
    {
        //OS_randombytes((void *)hash.uints,sizeof(basilisktag));
        hash.uints[0] = rand();
        vcalc_sha256(0,hash2.bytes,hash.bytes,sizeof(hash));
        if ( bits256_cmp(threshold,hash2) > 0 )
            break;
    }
    iguana_rwnum(0,(void *)hash.uints,sizeof(basilisktag),&basilisktag);
    iguana_rwnum(1,&data[-sizeof(basilisktag)],sizeof(basilisktag),&basilisktag);
    char str[65],str2[65]; printf("found hash after numiters.%d %s vs %s basilisktag.%u\n",numiters,bits256_str(str,threshold),bits256_str(str2,hash2),basilisktag);
    return(basilisktag);
}

char *basilisk_addhexstr(char **ptrp,cJSON *valsobj,char *strbuf,int32_t strsize,uint8_t *data,int32_t datalen)
{
    *ptrp = 0;
    if ( data != 0 && datalen > 0 )
    {
        if ( jobj(valsobj,"data") != 0 )
        {
            printf("basilisk_addhexstr warning: already have data object\n");
            jdelete(valsobj,"data");
        }
        if ( (datalen<<1)+1 > strsize )
        {
            strbuf = calloc(1,(datalen << 1) + 1);
            *ptrp = (void *)strbuf;
        }
        init_hexbytes_noT(strbuf,data,datalen);
        jaddstr(valsobj,"data",strbuf);
    } else return(0);
    return(strbuf);
}

uint8_t *get_dataptr(uint8_t **ptrp,int32_t *datalenp,uint8_t *space,int32_t spacesize,char *hexstr)
{
    *ptrp = 0; uint8_t *data = 0;
    if ( hexstr != 0 && (*datalenp= is_hexstr(hexstr,0)) > 0 )
    {
        *datalenp >>= 1;
        if ( *datalenp <= spacesize )
            data = space;
        else *ptrp = data = calloc(1,*datalenp);
        decode_hex(data,*datalenp,hexstr);
    }
    return(data);
}

uint8_t *basilisk_jsondata(void **ptrp,uint8_t *space,int32_t spacesize,int32_t *datalenp,char *symbol,cJSON *sendjson,uint32_t basilisktag)
{
    char *sendstr,*hexstr; uint8_t *data; bits256 pubkey; int32_t i,datalen,hexlen=0,extrasize,havepubkey=1;
    if ( jobj(sendjson,"coin") == 0 )
        jaddstr(sendjson,"coin",symbol);
    if ( jobj(sendjson,"pubkey") != 0 )
    {
        havepubkey = 1;
        pubkey = jbits256(sendjson,"pubkey");
    }
    if ( (hexstr= jstr(sendjson,"data")) != 0 && (hexlen= is_hexstr(hexstr,0)) > 0 )
        hexlen >>= 1;
    extrasize = (int32_t)(sizeof(struct iguana_msghdr) + sizeof(basilisktag));
    *ptrp = 0;
    sendstr = jprint(sendjson,0);
    datalen = (int32_t)strlen(sendstr) + 1;
    if ( (datalen + extrasize + hexlen + havepubkey*(sizeof(pubkey)+1)) <= spacesize )
        data = space;
    else
    {
        data = calloc(1,datalen + extrasize + hexlen + havepubkey*(sizeof(pubkey)+1));
        *ptrp = data;
    }
    data += extrasize;
    memcpy(data,sendstr,datalen);
    free(sendstr);
    if ( havepubkey != 0 || hexlen != 0 )
    {
        if ( (data[datalen++]= havepubkey) != 0 )
        {
            for (i=0; i<32; i++)
                data[datalen++] = pubkey.bytes[i];
        }
    }
    if ( hexlen > 0 )
    {
        decode_hex(&data[datalen],hexlen,hexstr);
        datalen += hexlen;
    }
    *datalenp = datalen;
    return(data);
}

char *basilisk_finish(struct basilisk_item *ptr,int32_t besti,char *errstr)
{
    char *retstr = 0; struct basilisk_item *parent;
    if ( ptr->retstr != 0 )
        return(ptr->retstr);
    if ( besti >= 0 && besti < ptr->numresults )
    {
        retstr = ptr->results[besti];
        ptr->results[besti] = 0;
    } else printf("besti.%d vs numresults.%d retstr.%p\n",besti,ptr->numresults,retstr);
    if ( retstr == 0 )
        retstr = clonestr(errstr);
    ptr->retstr = retstr;
    ptr->finished = (uint32_t)time(NULL);
    if ( (parent= ptr->parent) != 0 )
    {
        ptr->parent = 0;
        parent->childrendone++;
    }
    return(retstr);
}

struct basilisk_item *basilisk_itemcreate(struct supernet_info *myinfo,char *CMD,char *symbol,uint32_t basilisktag,int32_t minresults,cJSON *vals,int32_t timeoutmillis,void *metricfunc)
{
    struct basilisk_item *ptr;
    ptr = calloc(1,sizeof(*ptr));
    ptr->basilisktag = basilisktag;
    if ( (ptr->numrequired= minresults) == 0 )
        ptr->numrequired = 1;
    if ( (ptr->metricfunc= metricfunc) != 0 )
        ptr->vals = jduplicate(vals);
    strcpy(ptr->CMD,CMD);
    safecopy(ptr->symbol,symbol,sizeof(ptr->symbol));
    ptr->expiration = OS_milliseconds() + timeoutmillis;
    return(ptr);
}

/*int32_t basilisk_sendPUB(struct supernet_info *myinfo,uint32_t basilisktag,uint8_t *data,int32_t datalen) // data must be offset by sizeof(iguana_msghdr)+sizeof(basilisktag)
{
    int32_t i,j,r,r2,s,k,val,l,n=0; uint32_t *alreadysent; struct iguana_info *coin; struct iguana_peer *addr;
    alreadysent = calloc(IGUANA_MAXPEERS * IGUANA_MAXCOINS,sizeof(*alreadysent));
    r = rand(), r2 = rand();
    for (k=0; k<IGUANA_MAXCOINS; k++)
    {
        j = (r2 + k) % IGUANA_MAXCOINS;
        if ( (coin= Coins[j]) == 0 )
            continue;
        for (l=0; l<IGUANA_MAXPEERS; l++)
        {
            i = (l + r) % IGUANA_MAXPEERS;
            if ( (addr= &coin->peers.active[i]) != 0 && addr->ipbits != 0 && addr->usock >= 0 && addr->basilisk != 0 )
            {
                for (s=0; s<n; s++)
                    if ( alreadysent[s] == addr->ipbits )
                        break;
                if ( s == n )
                {
                    printf("pub (%s) addr->supernet.%u to (%s).%d\n",(char *)&data[4],addr->supernet,addr->ipaddr,addr->A.port);
                    if ( (val= iguana_queue_send(addr,0,&data[-sizeof(struct iguana_msghdr)],"SuperNETpub",datalen)) >= datalen )
                    {
                        alreadysent[n++] = (uint32_t)addr->ipbits;
                        if ( n >= IGUANA_MAXPEERS*IGUANA_MAXCOINS )
                            break;
                    }
                }
            }
        }
        if ( n >= IGUANA_MAXPEERS*IGUANA_MAXCOINS )
            break;
    }
    free(alreadysent);
    return(n);
}*/

int32_t basilisk_sendcmd(struct supernet_info *myinfo,char *destipaddr,char *type,uint32_t *basilisktagp,int32_t encryptflag,int32_t delaymillis,uint8_t *data,int32_t datalen,int32_t fanout,uint32_t nBits) // data must be offset by sizeof(iguana_msghdr)+sizeof(basilisktag)
{
    int32_t i,j,r,r2,k,l,s,val,n=0,offset,havepubkey=0,retval = -1; char cmd[12]; struct iguana_info *coin; struct iguana_peer *addr; bits256 pubkey; uint32_t *alreadysent;
    if ( fanout <= 0 )
        fanout = BASILISK_MINFANOUT;
    else if ( fanout > BASILISK_MAXFANOUT )
        fanout = BASILISK_MAXFANOUT;
    if ( type == 0 )
        type = "BTCD";
    if ( strlen(type) > 3 )
    {
        printf("basilisk_sendcmd illegal type(%s)\n",type);
        return(-1);
    }
    if ( destipaddr != 0 )
    {
        if ( destipaddr[0] == 0 )
            destipaddr = 0; // broadcast
        else if ( strcmp(destipaddr,"127.0.0.1") == 0 )
        {
            printf("return after locally basilisk_msgprocess\n");
            pubkey = GENESIS_PUBKEY;
            if ( datalen > 0 )
            {
                if ( (havepubkey= *data) != 0 )
                {
                    for (i=0; i<32; i++)
                        pubkey.bytes[i] = data[i + 1];
                }
            }
            offset = (int32_t)(havepubkey * (1 + sizeof(bits256)));
            basilisk_msgprocess(myinfo,0,0,type,*basilisktagp,data+offset,datalen-offset,pubkey);
            return(0);
        }
    }
    alreadysent = calloc(IGUANA_MAXPEERS * IGUANA_MAXCOINS,sizeof(*alreadysent));
    iguana_rwnum(1,&data[-sizeof(*basilisktagp)],sizeof(*basilisktagp),basilisktagp);
    if ( nBits != 0 && *basilisktagp == 0 )
        *basilisktagp = basilisk_calcnonce(myinfo,data,datalen,nBits);
    data -= sizeof(*basilisktagp), datalen += sizeof(*basilisktagp);
    memset(cmd,0,sizeof(cmd));
    sprintf(cmd,"SuperNET%s",type);
    r = rand(), r2 = rand();
    for (k=0; k<IGUANA_MAXCOINS; k++)
    {
        j = (r2 + k) % IGUANA_MAXCOINS;
        if ( (coin= Coins[j]) == 0 || coin->peers == 0 )
            continue;
        if ( coin->RELAYNODE == 0 && coin->VALIDATENODE == 0 )
            cmd[0] = 's';
        else cmd[0] = 'S';
        for (l=0; l<IGUANA_MAXPEERS; l++)
        {
            i = (l + r) % IGUANA_MAXPEERS;
            if ( (addr= &coin->peers->active[i]) != 0 && addr->usock >= 0 )
            {
                for (s=0; s<n; s++)
                    if ( alreadysent[s] == addr->ipbits )
                        break;
                //printf("%s s.%d vs n.%d\n",addr->ipaddr,s,n);
                if ( s == n && (addr->supernet != 0 || addr->basilisk != 0) && (destipaddr == 0 || strcmp(addr->ipaddr,destipaddr) == 0) )
                {
                    printf("[%s] send %s.(%s) addr->supernet.%u to (%s).%d destip.%s\n",cmd,type,(char *)&data[4],addr->supernet,addr->ipaddr,addr->A.port,destipaddr!=0?destipaddr:"broadcast");
                    if ( encryptflag != 0 && bits256_nonz(addr->pubkey) != 0 )
                    {
                        void *ptr; uint8_t *cipher,space[8192]; int32_t cipherlen; bits256 privkey;
                        cmd[6] = 'e', cmd[7] = 't';
                        memset(privkey.bytes,0,sizeof(privkey));
                        if ( (cipher= SuperNET_ciphercalc(&ptr,&cipherlen,&privkey,&addr->pubkey,data,datalen,space,sizeof(space))) != 0 )
                        {
                            if ( (val= iguana_queue_send(addr,delaymillis,&cipher[-sizeof(struct iguana_msghdr)],cmd,cipherlen)) >= cipherlen )
                                n++;
                            if ( ptr != 0 )
                                free(ptr);
                        }
                    }
                    else
                    {
                        cmd[6] = 'E', cmd[7] = 'T';
                        if ( (val= iguana_queue_send(addr,delaymillis,&data[-sizeof(struct iguana_msghdr)],cmd,datalen)) >= datalen )
                        {
                            alreadysent[n++] = (uint32_t)addr->ipbits;
                            if ( n >= IGUANA_MAXPEERS*IGUANA_MAXCOINS )
                                break;
                        }
                    }
                    if ( destipaddr != 0 || (fanout > 0 && n >= fanout) )
                    {
                        free(alreadysent);
                        return(val);
                    }
                    else if ( val > retval )
                        retval = val;
                }
            }
        }
        if ( n >= IGUANA_MAXPEERS*IGUANA_MAXCOINS )
            break;
    }
    free(alreadysent);
    return(n);
}

void basilisk_p2p(void *_myinfo,void *_addr,int32_t *delaymillisp,char *senderip,uint8_t *data,int32_t datalen,char *type,int32_t encrypted)
{
    uint32_t ipbits,basilisktag; int32_t i,havepubkey,msglen,len=0; void *ptr = 0; uint8_t space[8192]; bits256 senderpub,pubkey,hash,hash2; struct supernet_info *myinfo = _myinfo;
    pubkey = GENESIS_PUBKEY;
    if ( encrypted != 0 )
    {
        printf("encrypted basilisk_p2p.(%s) from %s\n",type,senderip!=0?senderip:"?");
        memset(senderpub.bytes,0,sizeof(senderpub));
        if ( (data= SuperNET_deciphercalc(&ptr,&msglen,myinfo->privkey,senderpub,data,datalen,space,sizeof(space))) == 0 )
        {
            printf("basilisk_p2p decrytion error\n");
            return;
        } else datalen = msglen;
    }
    if ( senderip != 0 && senderip[0] != 0 && strcmp(senderip,"127.0.0.1") != 0 )
        ipbits = (uint32_t)calc_ipbits(senderip);
    else ipbits = 0;
    len += iguana_rwnum(0,&data[len],sizeof(basilisktag),&basilisktag);
    if ( datalen > len && (havepubkey= data[len]) != 0 )
    {
        vcalc_sha256(0,hash.bytes,&data[len],datalen - len);
        hash.uints[0] = basilisktag;
        vcalc_sha256(0,hash2.bytes,hash.bytes,sizeof(hash));
        char str[65]; printf("tag.%u %s\n",basilisktag,bits256_str(str,hash2));
        for (i=0; i<32; i++)
            pubkey.bytes[i] = data[len + i + 1];
    }
    if ( bits256_nonz(pubkey) == 0 )
        pubkey = GENESIS_PUBKEY;
    basilisk_msgprocess(myinfo,_addr,ipbits,type,basilisktag,&data[len],datalen - len,pubkey);
    if ( ptr != 0 )
        free(ptr);
}

void basilisk_sendback(struct supernet_info *myinfo,char *symbol,char *remoteaddr,uint32_t basilisktag,char *retstr)
{
    uint8_t *data,space[4096]; void *allocptr; cJSON *valsobj; int32_t datalen,encryptflag=0,delaymillis=0;
    printf("retstr.(%s) -> remote.(%s) basilisktag.%u\n",retstr,remoteaddr,basilisktag);
    if ( retstr != 0 && remoteaddr != 0 && remoteaddr[0] != 0 && strcmp(remoteaddr,"127.0.0.1") != 0 )
    {
        if ( (valsobj= cJSON_Parse(retstr)) != 0 )
        {
            data = basilisk_jsondata(&allocptr,space,sizeof(space),&datalen,symbol,valsobj,basilisktag);
            basilisk_sendcmd(myinfo,remoteaddr,"RET",&basilisktag,encryptflag,delaymillis,data,datalen,0,0);
            if ( allocptr != 0 )
                free(allocptr);
            free_json(valsobj);
        }
    }
}

char *basilisk_waitresponse(struct supernet_info *myinfo,char *CMD,char *symbol,char *remoteaddr,struct basilisk_item *Lptr,struct basilisk_item *ptr)
{
    char *retstr = 0;
    if ( ptr == Lptr )
    {
        if ( (retstr= Lptr->retstr) == 0 )
            retstr = clonestr("{\"result\":\"null return from local basilisk_issuecmd\"}");
        ptr = basilisk_itemcreate(myinfo,CMD,symbol,Lptr->basilisktag,Lptr->numrequired,Lptr->vals,OS_milliseconds() - Lptr->expiration,Lptr->metricfunc);
        queue_enqueue("submitQ",&myinfo->basilisks.submitQ,&ptr->DL,0);
    }
    else
    {
        queue_enqueue("submitQ",&myinfo->basilisks.submitQ,&ptr->DL,0);
        while ( OS_milliseconds() < ptr->expiration )
        {
            //if ( (retstr= basilisk_iscomplete(ptr)) != 0 )
            if ( (retstr= ptr->retstr) != 0 )
                break;
            usleep(50000);
        }
        if ( retstr == 0 )
            retstr = basilisk_finish(ptr,-1,"{\"error\":\"basilisk timeout\"}");
    }
    basilisk_sendback(myinfo,symbol,remoteaddr,ptr->basilisktag,retstr);
    return(retstr);
}

struct basilisk_item *basilisk_issueremote(struct supernet_info *myinfo,int32_t *numsentp,char *CMD,char *symbol,cJSON *valsobj,int32_t fanout,int32_t minresults,uint32_t basilisktag,int32_t timeoutmillis,void *_metricfunc,char *retstr,int32_t encryptflag,int32_t delaymillis,uint32_t nBits)
{
    struct basilisk_item *ptr; void *allocptr; uint8_t *data,space[4096]; int32_t datalen; basilisk_metricfunc metricfunc = _metricfunc;
    ptr = basilisk_itemcreate(myinfo,CMD,symbol,basilisktag,minresults,valsobj,timeoutmillis,metricfunc);
    ptr->nBits = nBits;
    *numsentp = 0;
    if ( retstr != 0 )
    {
        ptr->results[0] = ptr->retstr = retstr;
        ptr->numresults = ptr->numrequired;
        ptr->metrics[0] = (*metricfunc)(myinfo,ptr,retstr);
        ptr->finished = (uint32_t)time(NULL);
    }
    else
    {
        data = basilisk_jsondata(&allocptr,space,sizeof(space),&datalen,symbol,valsobj,basilisktag);
        *numsentp = ptr->numsent = basilisk_sendcmd(myinfo,0,CMD,&ptr->basilisktag,encryptflag,delaymillis,data,datalen,1,ptr->nBits);
        if ( allocptr != 0 )
            free(allocptr);
    }
    return(ptr);
}

struct basilisk_item *basilisk_requestservice(struct supernet_info *myinfo,char *CMD,cJSON *valsobj,bits256 pubkey,uint8_t *data,int32_t datalen,uint32_t nBits)
{
    int32_t minresults,timeoutmillis,numsent,delaymillis,encryptflag; struct basilisk_item *ptr; char buf[4096],*str = 0;
    basilisk_addhexstr(&str,valsobj,buf,sizeof(buf),data,datalen);
    if ( jobj(valsobj,"pubkey") != 0 )
        jdelete(valsobj,"pubkey");
    if ( bits256_cmp(pubkey,GENESIS_PUBKEY) != 0 && bits256_nonz(pubkey) != 0 )
        jaddbits256(valsobj,"pubkey",pubkey);
    if ( (minresults= jint(valsobj,"minresults")) <= 0 )
        minresults = 1;
    if ( (timeoutmillis= jint(valsobj,"timeout")) == 0 )
        timeoutmillis = BASILISK_TIMEOUT;
    encryptflag = jint(valsobj,"encrypt");
    delaymillis = jint(valsobj,"delay");
    ptr = basilisk_issueremote(myinfo,&numsent,CMD,"BTCD",valsobj,1,minresults,0,timeoutmillis,0,0,encryptflag,delaymillis,nBits);
    return(ptr);
}

char *basilisk_standardservice(char *CMD,struct supernet_info *myinfo,bits256 hash,cJSON *valsobj,char *hexstr,int32_t blockflag) // client side
{
    uint32_t nBits = 0; uint8_t space[8192],*allocptr=0,*data = 0; struct basilisk_item *ptr,Lptr; int32_t datalen = 0; cJSON *retjson;
    retjson = cJSON_CreateObject();
    data = get_dataptr(&allocptr,&datalen,space,sizeof(space),hexstr);
    ptr = basilisk_requestservice(myinfo,CMD,valsobj,hash,data,datalen,nBits);
    if ( allocptr != 0 )
        free(allocptr);
    if ( ptr != 0 )
    {
        if ( blockflag != 0 )
        {
            if ( ptr->expiration <= OS_milliseconds() )
                ptr->expiration = OS_milliseconds() + BASILISK_TIMEOUT;
            ptr->vals = jduplicate(valsobj);
            strcpy(ptr->symbol,"BTCD");
            strcpy(ptr->CMD,CMD);
            return(basilisk_waitresponse(myinfo,CMD,"BTCD",0,&Lptr,ptr));
        }
        else if ( ptr->numsent > 0 )
        {
            jaddstr(retjson,"result","success");
            jaddnum(retjson,"numsent",ptr->numsent);
        } else jaddstr(retjson,"error","didnt find any nodes to send to");
    } else jaddstr(retjson,"error","couldnt create basilisk item");
    return(jprint(retjson,1));
}

#include "basilisk_bitcoin.c"
#include "basilisk_nxt.c"
#include "basilisk_ether.c"
#include "basilisk_waves.c"
#include "basilisk_lisk.c"
#include "basilisk_CMD.c"

void basilisk_functions(struct iguana_info *coin,int32_t protocol)
{
    coin->protocol = protocol;
    switch ( protocol )
    {
        case IGUANA_PROTOCOL_BITCOIN:
            coin->basilisk_balances = basilisk_bitcoinbalances;
            coin->basilisk_rawtx = basilisk_bitcoinrawtx;
            coin->basilisk_rawtxmetric = basilisk_bitcoin_rawtxmetric;
            coin->basilisk_value = basilisk_bitcoinvalue;
            coin->basilisk_valuemetric = basilisk_bitcoin_valuemetric;
            break;
        /*case IGUANA_PROTOCOL_IOTA:
            coin->basilisk_balances = basilisk_iotabalances;
            coin->basilisk_rawtx = basilisk_iotarawtx;
            break;
        case IGUANA_PROTOCOL_NXT:
            coin->basilisk_balances = basilisk_nxtbalances;
            coin->basilisk_rawtx = basilisk_nxtrawtx;
            break;
        case IGUANA_PROTOCOL_ETHER:
            coin->basilisk_balances = basilisk_etherbalances;
            coin->basilisk_rawtx = basilisk_etherrawtx;
            break;
        case IGUANA_PROTOCOL_WAVES:
            coin->basilisk_balances = basilisk_wavesbalances;
            coin->basilisk_rawtx = basilisk_wavesrawtx;
            break;
        case IGUANA_PROTOCOL_LISK:
            coin->basilisk_balances = basilisk_liskbalances;
            coin->basilisk_rawtx = basilisk_liskrawtx;
            break;*/
    }
}

int32_t basilisk_besti(struct basilisk_item *ptr)
{
    int32_t i,besti = -1; double metric,bestmetric=-1.;
    for (i=0; i<ptr->numresults; i++)
    {
        if ( (metric= ptr->metrics[i]) > 0. )
        {
            if ( (ptr->metricdir < 0 && (bestmetric < 0. || metric < bestmetric)) || (ptr->metricdir > 0 && (bestmetric < 0. || metric > bestmetric)) || (ptr->metricdir == 0 && bestmetric < 0.) )
            {
                bestmetric = metric;
                besti = i;
            }
        }
    }
    if ( besti >= 0 )
    {
        for (ptr->numexact=i=0; i<ptr->numresults; i++)
            if ( fabs(ptr->metrics[i] - bestmetric) < SMALLVAL )
                ptr->numexact++;
    }
    return(besti);
}

char *basilisk_iscomplete(struct basilisk_item *ptr)
{
    int32_t i,numvalid,besti=-1; char *errstr = 0,*retstr = 0;
    if ( ptr->childrendone < ptr->numchildren )
        return(0);
    if ( ptr->retstr != 0 || ptr->finished != 0 )
        return(ptr->retstr);
    if ( (numvalid= ptr->numresults) >= ptr->numrequired )
    {
        for (i=numvalid=0; i<ptr->numresults; i++)
        {
            if ( ptr->metrics[i] != 0. )
                numvalid++;
        }
    }
    if ( numvalid < ptr->numrequired )
    {
        //printf("%u: numvalid.%d < required.%d m %f\n",ptr->basilisktag,numvalid,ptr->numrequired,ptr->metrics[0]);
        return(0);
    }
    if ( ptr->uniqueflag == 0 && ptr->numexact != ptr->numresults && ptr->numexact < (ptr->numresults >> 1) )
        besti = -1, errstr = "{\"error\":\"basilisk non-consensus results\"}";
    else besti = basilisk_besti(ptr), errstr = "{\"error\":\"basilisk no valid results\"}";
    //printf("%u complete besti.%d\n",ptr->basilisktag,besti);
    retstr = basilisk_finish(ptr,besti,errstr);
    //printf("%u besti.%d numexact.%d numresults.%d -> (%s)\n",ptr->basilisktag,besti,ptr->numexact,ptr->numresults,retstr);
    return(retstr);
}

struct basilisk_item *basilisk_issuecmd(struct basilisk_item *Lptr,basilisk_func func,basilisk_metricfunc metricfunc,struct supernet_info *myinfo,char *remoteaddr,uint32_t basilisktag,char *symbol,int32_t timeoutmillis,cJSON *vals)
{
    struct iguana_info *coin; struct basilisk_item *ptr;
    memset(Lptr,0,sizeof(*Lptr));
    if ( (coin= iguana_coinfind(symbol)) != 0 )
    {
        if ( func != 0 )
        {
            if ( (ptr= (*func)(Lptr,myinfo,coin,remoteaddr,basilisktag,timeoutmillis,vals)) != 0 )
            {
                if ( (ptr->metricfunc= metricfunc) != 0 )
                    ptr->vals = jduplicate(vals);
                strcpy(ptr->symbol,symbol);
                ptr->basilisktag = basilisktag;
                ptr->expiration = OS_milliseconds() + timeoutmillis;
                return(ptr);
            } else Lptr->retstr = clonestr("{\"error\":\"error issuing basilisk command\"}");
        } else Lptr->retstr = clonestr("{\"error\":\"null basilisk function\"}");
    } else Lptr->retstr = clonestr("{\"error\":\"error missing coin\"}");
    return(Lptr);
}

char *basilisk_check(int32_t *timeoutmillisp,uint32_t *basilisktagp,char *symbol,cJSON *vals)
{
    if ( symbol != 0 && symbol[0] != 0 && vals != 0 )
    {
        if ( *basilisktagp == 0 )
            *basilisktagp = rand();
        if ( (*timeoutmillisp= jint(vals,"timeout")) < 0 )
            *timeoutmillisp = BASILISK_TIMEOUT;
        return(0);
    } else return(clonestr("{\"error\":\"missing activecoin or vals\"}"));
}

char *basilisk_standardcmd(struct supernet_info *myinfo,char *CMD,char *activecoin,char *remoteaddr,uint32_t basilisktag,cJSON *vals,basilisk_func func,basilisk_metricfunc metric)
{
    char *retstr; struct basilisk_item *ptr,Lptr; int32_t timeoutmillis; struct iguana_info *coin;
    if ( (retstr= basilisk_check(&timeoutmillis,&basilisktag,activecoin,vals)) == 0 )
    {
        if ( (coin= iguana_coinfind(activecoin)) != 0 )
        {
            if ( (ptr= basilisk_issuecmd(&Lptr,func,metric,myinfo,remoteaddr,basilisktag,activecoin,timeoutmillis,vals)) != 0 )
                return(basilisk_waitresponse(myinfo,CMD,coin->symbol,remoteaddr,&Lptr,ptr));
            else return(clonestr("{\"error\":\"null return from basilisk_issuecmd\"}"));
        } else return(clonestr("{\"error\":\"couldnt get coin\"}"));
    } else return(retstr);
}

char *_basilisk_value(struct supernet_info *myinfo,struct iguana_info *coin,struct iguana_peer *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen)
{
    return(basilisk_standardcmd(myinfo,"VAL",coin->symbol,remoteaddr,basilisktag,valsobj,coin->basilisk_value,coin->basilisk_valuemetric));
}

char *_basilisk_balances(struct supernet_info *myinfo,struct iguana_info *coin,struct iguana_peer *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen)
{
    return(basilisk_standardcmd(myinfo,"BAL",coin->symbol,remoteaddr,basilisktag,valsobj,coin->basilisk_balances,coin->basilisk_balancesmetric));
}

char *_basilisk_rawtx(struct supernet_info *myinfo,struct iguana_info *coin,struct iguana_peer *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen)
{
    char *retstr,strbuf[4096],*str = 0;
    printf("remote rawtx.(%s)\n",jprint(valsobj,0));
    basilisk_addhexstr(&str,valsobj,strbuf,sizeof(strbuf),data,datalen);
    retstr = basilisk_rawtx(myinfo,coin,0,remoteaddr,basilisktag,valsobj,coin->symbol);
    if ( str != 0 )
        free(str);
    return(retstr);
}

char *_basilisk_result(struct supernet_info *myinfo,struct iguana_info *coin,struct iguana_peer *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen)
{
    char *retstr,strbuf[4096],*str = 0;
    basilisk_addhexstr(&str,valsobj,strbuf,sizeof(strbuf),data,datalen);
    retstr = basilisk_result(myinfo,coin,0,remoteaddr,basilisktag,valsobj);
    if ( str != 0 )
        free(str);
    return(retstr);
}

char *basilisk_checkrawtx(int32_t *timeoutmillisp,uint32_t *basilisktagp,char *symbol,cJSON *vals)
{
    cJSON *addresses=0; char *changeaddr,*spendscriptstr; int32_t i,n;
    *timeoutmillisp = -1;
    changeaddr = jstr(vals,"changeaddr");
    spendscriptstr = jstr(vals,"spendscript");
    addresses = jarray(&n,vals,"addresses");
    if ( addresses == 0 || changeaddr == 0 || changeaddr[0] == 0 )
        return(clonestr("{\"error\":\"invalid addresses[] or changeaddr\"}"));
    else
    {
        for (i=0; i<n; i++)
            if ( strcmp(jstri(addresses,i),changeaddr) == 0 )
                return(clonestr("{\"error\":\"changeaddr cant be in addresses[]\"}"));
    }
    if ( spendscriptstr != 0 && spendscriptstr[0] != 0 )
        return(basilisk_check(timeoutmillisp,basilisktagp,symbol,vals));
    else
    {
        printf("vals.(%s)\n",jprint(vals,0));
        return(clonestr("{\"error\":\"missing spendscript\"}"));
    }
}

#include "../includes/iguana_apidefs.h"
#include "../includes/iguana_apideclares.h"

INT_ARRAY_STRING(basilisk,balances,basilisktag,vals,activecoin)
{
    return(basilisk_standardcmd(myinfo,"BAL",activecoin,remoteaddr,basilisktag,vals,coin->basilisk_balances,coin->basilisk_balancesmetric));
}

INT_ARRAY_STRING(basilisk,value,basilisktag,vals,activecoin)
{
    return(basilisk_standardcmd(myinfo,"VAL",activecoin,remoteaddr,basilisktag,vals,coin->basilisk_value,coin->basilisk_valuemetric));
}

INT_ARRAY_STRING(basilisk,rawtx,basilisktag,vals,activecoin)
{
    char *retstr; struct basilisk_item *ptr,Lptr; int32_t timeoutmillis;
    if ( (retstr= basilisk_checkrawtx(&timeoutmillis,(uint32_t *)&basilisktag,activecoin,vals)) == 0 )
    {
        coin = iguana_coinfind(activecoin);
        if ( coin != 0 && (ptr= basilisk_issuecmd(&Lptr,coin->basilisk_rawtx,coin->basilisk_rawtxmetric,myinfo,remoteaddr,basilisktag,activecoin,timeoutmillis,vals)) != 0 )
        {
            if ( (ptr->numrequired= juint(vals,"numrequired")) == 0 )
                ptr->numrequired = 1;
            ptr->uniqueflag = 1;
            ptr->metricdir = -1;
            return(basilisk_waitresponse(myinfo,"RAW",coin->symbol,remoteaddr,&Lptr,ptr));
        } else return(clonestr("{\"error\":\"error issuing basilisk rawtx\"}"));
    } else return(retstr);
}

INT_AND_ARRAY(basilisk,result,basilisktag,vals)
{
    struct basilisk_item *ptr;
    if ( vals != 0 )
    {
        ptr = calloc(1,sizeof(*ptr));
        ptr->retstr = jprint(vals,0);
        ptr->basilisktag = basilisktag;
        printf("(%s) -> Q.%u results vals.(%s)\n",ptr->CMD,basilisktag,ptr->retstr);
        queue_enqueue("resultsQ",&myinfo->basilisks.resultsQ,&ptr->DL,0);
        return(clonestr("{\"result\":\"queued basilisk return\"}"));
    } else printf("null vals.(%s) or no hexmsg.%p\n",jprint(vals,0),vals);
    return(clonestr("{\"error\":\"no hexmsg to return\"}"));
}

HASH_ARRAY_STRING(basilisk,addrelay,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("ADD",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,dispatch,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("RUN",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,publish,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("PUB",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,subscribe,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("SUB",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,forward,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("HOP",myinfo,pubkey,vals,hexstr,0));
}

HASH_ARRAY_STRING(basilisk,mailbox,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("BOX",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNcreate,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("VPN",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNjoin,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("ARC",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNmessage,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("GAB",myinfo,pubkey,vals,hexstr,0));
}

HASH_ARRAY_STRING(basilisk,VPNbroadcast,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("SAY",myinfo,pubkey,vals,hexstr,0));
}

HASH_ARRAY_STRING(basilisk,VPNreceive,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("EAR",myinfo,pubkey,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNlogout,pubkey,vals,hexstr)
{
    return(basilisk_standardservice("END",myinfo,pubkey,vals,hexstr,0));
}

#include "../includes/iguana_apiundefs.h"

void basilisk_msgprocess(struct supernet_info *myinfo,void *addr,uint32_t senderipbits,char *type,uint32_t basilisktag,uint8_t *data,int32_t datalen,bits256 pubkey)
{
    cJSON *valsobj; char *symbol,*retstr=0,remoteaddr[64],CMD[4],cmd[4]; int32_t origlen,from_basilisk,i,timeoutmillis,numrequired,jsonlen; uint8_t *origdata; struct iguana_info *coin=0;
    static basilisk_servicefunc *basilisk_services[][2] =
    {
        { (void *)"RUN", &basilisk_respond_dispatch },   // higher level protocol handler, pass through
        { (void *)"BYE", &basilisk_respond_goodbye },    // disconnect
       
        // private chains
        { (void *)"NEW", &basilisk_respond_newprivatechain }, // creates new virtual private chain
        { (void *)"SEQ", &basilisk_respond_hashstamps }, // BTCD and BTC recent hashes from timestamp
        { (void *)"VTX", &basilisk_respond_privatetx },   // adding field to one relay propagates to all others
        { (void *)"BLK", &basilisk_respond_privateblock },   // any relay can be queried

        // unencrypted low level functions, used by higher level protocols and virtual network funcs
        { (void *)"ADD", &basilisk_respond_addrelay },   // relays register with each other bus
        { (void *)"PUB", &basilisk_respond_publish },    // adds to global list of published items
        { (void *)"SUB", &basilisk_respond_subscribe },  // subscribes to one or all published items
        
        // encrypted data for jumblr
        { (void *)"HOP", &basilisk_respond_forward },    // message forwarding
        { (void *)"BOX", &basilisk_respond_mailbox },    // create/send/check mailbox pubkey
        
        // small virtual private network
        { (void *)"VPN", &basilisk_respond_VPNcreate },  // create virtual network's hub via privkey
        { (void *)"ARC", &basilisk_respond_VPNjoin },    // join
        { (void *)"GAB", &basilisk_respond_VPNmessage }, // private message
        { (void *)"SAY", &basilisk_respond_VPNbroadcast }, // broadcast
        { (void *)"EAR", &basilisk_respond_VPNreceive }, // network receive (via poll)
        { (void *)"END", &basilisk_respond_VPNlogout },  // logout
    };
    static basilisk_coinfunc *basilisk_coinservices[][2] =
    {
        { (void *)"RAW", &_basilisk_rawtx },
        { (void *)"VAL", &_basilisk_value },
        { (void *)"BAL", &_basilisk_balances },
    };
    strncpy(CMD,type,3), CMD[3] = cmd[3] = 0;
    if ( isupper((int32_t)CMD[0]) != 0 && isupper((int32_t)CMD[1]) != 0 && isupper((int32_t)CMD[2]) != 0 )
        from_basilisk = 1;
    else from_basilisk = 0;
    origdata = data;
    origlen = datalen;
    for (i=0; i<3; i++)
    {
        CMD[i] = toupper((int32_t)CMD[i]);
        cmd[i] = tolower((int32_t)cmd[i]);
    }
    //printf("MSGPROCESS.(%s)\n",(char *)data);
    if ( (valsobj= cJSON_Parse((char *)data)) != 0 )
    {
        jsonlen = (int32_t)strlen((char *)data) + 1;
        if ( datalen > jsonlen )
        {
            data += jsonlen, datalen -= jsonlen;
            if ( *data++ != 0 )
                data += sizeof(pubkey), datalen -= sizeof(pubkey);
        } else data = 0, datalen = 0;
        if ( (symbol= jstr(valsobj,"coin")) == 0 )
            symbol = "BTCD";
        timeoutmillis = jint(valsobj,"timeout");
        if ( (numrequired= jint(valsobj,"numrequired")) == 0 )
            numrequired = 1;
        if ( jobj(valsobj,"coin") != 0 )
            coin = iguana_coinfind(jstr(valsobj,"coin"));
        if ( senderipbits != 0 )
            expand_ipbits(remoteaddr,senderipbits);
        else remoteaddr[0] = 0;
        for (i=0; i<sizeof(basilisk_services)/sizeof(*basilisk_services); i++) // iguana node
        {
            if ( strcmp((char *)basilisk_services[i][0],type) == 0 )
            {
                if ( myinfo->IAMRELAY != 0 ) // iguana node
                {
                    if ( from_basilisk != 0 )
                        basilisk_sendcmd(myinfo,0,cmd,&basilisktag,0,0,origdata,origlen,-1,0); // to other iguanas
                    if ( (retstr= (*basilisk_services[i][1])(myinfo,type,addr,remoteaddr,basilisktag,valsobj,data,datalen,pubkey,from_basilisk)) != 0 )
                    {
                        printf("from_basilisk.%d ret.(%s)\n",from_basilisk,retstr);
                        if ( from_basilisk != 0 )
                            basilisk_sendback(myinfo,symbol,remoteaddr,basilisktag,retstr);
                        if ( retstr != 0 )
                            free(retstr);
                    }
                } else printf("non-relay got unexpected.(%s)\n",type);
                free_json(valsobj);
                return;
            }
        }
        if ( coin != 0 )
        {
            if ( coin->RELAYNODE != 0 || coin->VALIDATENODE != 0 ) // iguana node
            {
                for (i=0; i<sizeof(basilisk_coinservices)/sizeof(*basilisk_coinservices); i++)
                    if ( strcmp((char *)basilisk_coinservices[i][0],type) == 0 )
                    {
                        retstr = (*basilisk_coinservices[i][1])(myinfo,coin,addr,remoteaddr,basilisktag,valsobj,data,datalen);
                        printf("from_basilisk.%d ret.(%s)\n",from_basilisk,retstr);
                        if ( from_basilisk != 0 )
                            basilisk_sendback(myinfo,symbol,remoteaddr,basilisktag,retstr);
                        break;
                    }
            }
            else // basilisk node
            {
                if ( strcmp(type,"RET") == 0 )
                {
                    retstr = _basilisk_result(myinfo,coin,addr,remoteaddr,basilisktag,valsobj,data,datalen);
                }
                else if ( strcmp(type,"ADD") == 0 )
                {
                    printf("new relay ADD.(%s) datalen.%d\n",jprint(valsobj,0),datalen);
                }
                else printf("basilisk node doenst handle.(%s)\n",type);
            }
        } else printf("basilisk_msgprocess no coin\n");
    }
    free_json(valsobj);
    if ( retstr != 0 )
        free(retstr);
}

void basilisks_loop(void *arg)
{
    basilisk_metricfunc metricfunc; struct iguana_info *btcd; struct basilisk_item *ptr,*tmp,*pending,*parent; int32_t i,iter,done,flag,n; cJSON *valsobj,*retjson; uint32_t now; struct supernet_info *myinfo = arg;
    //uint8_t *blockspace; struct OS_memspace RAWMEM;
    //memset(&RAWMEM,0,sizeof(RAWMEM));
    //blockspace = calloc(1,IGUANA_MAXPACKETSIZE);
    iter = 0;
    while ( 1 )
    {
        iter++;
        if ( (btcd= iguana_coinfind("BTCD")) != 0 )
        {
            done = 3;
            if ( btcd->RELAYNODE != 0 || btcd->VALIDATENODE != 0 )
            {
                if ( (now= (uint32_t)time(NULL)) > btcd->SEQ.BTCD.lastupdate+10 )
                {
                    if ( basilisk_update("BTCD",now) >= 0 )
                        done &= ~1;
                    btcd->SEQ.BTCD.lastupdate = (uint32_t)time(NULL);
                }
            }
            if ( (now= (uint32_t)time(NULL)) > btcd->SEQ.BTC.lastupdate+30 )
            {
                if ( basilisk_update("BTC",now) >= 0 )
                    done &= ~2;
                btcd->SEQ.BTC.lastupdate = (uint32_t)time(NULL);
            }
            if ( done != 3 )
            {
                valsobj = cJSON_CreateObject();
                if ( btcd->RELAYNODE == 0 && btcd->VALIDATENODE == 0 )
                {
                    jaddnum(valsobj,"BTCD",btcd->SEQ.BTCD.numstamps+BASILISK_FIRSTPOSSIBLEBTCD);
                    basilisk_standardservice("SEQ",myinfo,GENESIS_PUBKEY,valsobj,0,0);
                }
                if ( (done & 2) == 0 )
                {
                    free_json(valsobj);
                    valsobj = cJSON_CreateObject();
                    jaddnum(valsobj,"BTC",btcd->SEQ.BTC.numstamps+BASILISK_FIRSTPOSSIBLEBTC);
                    basilisk_standardservice("SEQ",myinfo,GENESIS_PUBKEY,valsobj,0,0);
                }
                free_json(valsobj);
            }
        }
        //for (i=0; i<IGUANA_MAXCOINS; i++)
        //    if ( (coin= Coins[i]) != 0 && coin->RELAYNODE == 0 && coin->VALIDATENODE == 0 && coin->active != 0 && coin->chain->userpass[0] != 0 && coin->MAXPEERS == 1 )
        //        basilisk_bitcoinscan(coin,blockspace,&RAWMEM);
        if ( (ptr= queue_dequeue(&myinfo->basilisks.submitQ,0)) != 0 )
        {
            if ( ptr->finished == 0 )
                HASH_ADD(hh,myinfo->basilisks.issued,basilisktag,sizeof(ptr->basilisktag),ptr);
            else free(ptr);
            continue;
        }
        if ( (ptr= queue_dequeue(&myinfo->basilisks.resultsQ,0)) != 0 )
        {
            HASH_FIND(hh,myinfo->basilisks.issued,&ptr->basilisktag,sizeof(ptr->basilisktag),pending);
            if ( pending != 0 )
            {
                if ( (n= pending->numresults) < sizeof(pending->results)/sizeof(*pending->results) )
                {
                    pending->numresults++;
                    if ( (metricfunc= pending->metricfunc) == 0 )
                        pending->metrics[n] = n + 1;
                    else if ( (pending->metrics[n]= (*metricfunc)(myinfo,pending,ptr->retstr)) != 0. )
                        pending->childrendone++;
                    printf("%u Add results[%d] <- (%s) metric %f\n",pending->basilisktag,n,ptr->retstr,pending->metrics[n]);
                    pending->results[n] = ptr->retstr;
                    if ( strcmp(ptr->CMD,"SEQ") == 0 )
                    {
                        if ( (retjson= cJSON_Parse(ptr->retstr)) != 0 )
                        {
                            basilisk_seqresult(myinfo,ptr->retstr);
                            free_json(retjson);
                        }
                    }
                }
            }
            free(ptr);
            continue;
        }
        flag = 0;
        HASH_ITER(hh,myinfo->basilisks.issued,pending,tmp)
        {
            //printf("pending.%u numresults.%d m %f func.%p\n",pending->basilisktag,pending->numresults,pending->metrics[0],pending->metricfunc);
            if ( (metricfunc= pending->metricfunc) != 0 )
            {
                for (i=0; i<pending->numresults; i++)
                    if ( pending->metrics[i] == 0. && pending->results[i] != 0 )
                    {
                        if ( (pending->metrics[i]= (*metricfunc)(myinfo,pending,pending->results[i])) != 0 )
                            pending->childrendone++;
                        // printf("iter.%d %p.[%d] poll metrics.%u metric %f\n",iter,pending,i,pending->basilisktag,pending->metrics[i]);
                        flag++;
                    }
            }
            basilisk_iscomplete(pending);
            if ( OS_milliseconds() > pending->expiration )
            {
                if ( pending->finished == 0 )
                {
                    if ( (parent= pending->parent) != 0 )
                    {
                        pending->parent = 0;
                        parent->childrendone++;
                    }
                    pending->finished = (uint32_t)time(NULL);
                    if ( pending->retstr == 0 )
                        pending->retstr = clonestr("{\"error\":\"basilisk timeout\"}");
                    printf("timeout call metrics.%u lag %f - %f\n",pending->basilisktag,OS_milliseconds(),pending->expiration);
                    for (i=0; i<pending->numresults; i++)
                        if ( (metricfunc= pending->metricfunc) != 0 )
                            pending->metrics[i] = (*metricfunc)(myinfo,pending,pending->results[i]);
                }
            }
            if ( pending->finished != 0 && time(NULL) > pending->finished+60 )
            {
                if ( pending->dependents == 0 || pending->childrendone >= pending->numchildren )
                {
                    HASH_DELETE(hh,myinfo->basilisks.issued,pending);
                    if ( pending->dependents != 0 )
                        free(pending->dependents);
                    printf("HASH_DELETE free ptr.%u\n",pending->basilisktag);
                    for (i=0; i<pending->numresults; i++)
                        if ( pending->results[i] != 0 )
                            free(pending->results[i]);
                    if ( pending->vals != 0 )
                        free_json(pending->vals);
                    free(pending);
                    flag++;
                }
            }
        }
        if ( flag == 0 )
            usleep(50000);
        else usleep(10000);
    }
}

void basilisks_init(struct supernet_info *myinfo)
{
    //bits256 basiliskhash;
    iguana_initQ(&myinfo->basilisks.submitQ,"submitQ");
    iguana_initQ(&myinfo->basilisks.resultsQ,"resultsQ");
    //basiliskhash = calc_categoryhashes(0,"basilisk",0);
    //myinfo->basilisk_category = basiliskhash;
    //category_subscribe(myinfo,basiliskhash,GENESIS_PUBKEY);
    //category_processfunc(basiliskhash,GENESIS_PUBKEY,basilisk_hexmsg);
    //category_processfunc(basiliskhash,myinfo->myaddr.persistent,basilisk_hexmsg);
    myinfo->basilisks.launched = iguana_launch(iguana_coinfind("BTCD"),"basilisks_loop",basilisks_loop,myinfo,IGUANA_PERMTHREAD);
}
