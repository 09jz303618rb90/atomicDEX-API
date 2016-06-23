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

// included from basilisk.c

struct iguana_peer *basilisk_ensurerelay(struct supernet_info *myinfo,struct iguana_info *btcd,uint32_t ipbits)
{
    struct iguana_peer *addr; int32_t i;
    if ( (addr= iguana_peerfindipbits(btcd,ipbits,0)) == 0 )
    {
        if ( (addr= iguana_peerslot(btcd,ipbits,0)) != 0 )
        {
            printf("launch peer for relay\n");
            addr->isrelay = 1;
            myinfo->RELAYID = -1;
            for (i=0; i<myinfo->numrelays; i++)
                if ( myinfo->relaybits[i] == myinfo->myaddr.myipbits )
                {
                    myinfo->RELAYID = i;
                    break;
                }

            iguana_launch(btcd,"addrelay",iguana_startconnection,addr,IGUANA_CONNTHREAD);
        } else printf("error getting peerslot\n");
    } else addr->isrelay = 1;
    return(addr);
}

char *basilisk_addrelay_info(struct supernet_info *myinfo,uint8_t *pubkey33,uint32_t ipbits,bits256 pubkey)
{
    int32_t i; struct basilisk_relay *rp; struct iguana_info *btcd;
//return(clonestr("{\"error\":\"addrelay info disabled\"}"));
    if ( (btcd= iguana_coinfind("BTCD")) == 0 || ipbits == 0 )
        return(clonestr("{\"error\":\"add relay needs BTCD and ipbits\"}"));
    for (i=0; i<myinfo->numrelays; i++)
    {
        rp = &myinfo->relays[i];
        if ( ipbits == rp->ipbits )
        {
            if ( bits256_cmp(GENESIS_PUBKEY,pubkey) != 0 && bits256_nonz(pubkey) != 0 )
                rp->pubkey = pubkey;
            if ( pubkey33 != 0 && pubkey33[0] != 0 )
                memcpy(rp->pubkey33,pubkey33,33);
            //printf("updated relay[%d] %x\n",i,ipbits);
            return(clonestr("{\"error\":\"relay already there\"}"));
        }
    }
    if ( i >= sizeof(myinfo->relays)/sizeof(*myinfo->relays) )
        i = (rand() % (sizeof(myinfo->relays)/sizeof(*myinfo->relays)));
    printf("add relay[%d] <- %x\n",i,ipbits);
    rp = &myinfo->relays[i];
    rp->ipbits = ipbits;
    rp->addr = basilisk_ensurerelay(myinfo,btcd,rp->ipbits);
    if ( myinfo->numrelays < sizeof(myinfo->relays)/sizeof(*myinfo->relays) )
        myinfo->numrelays++;
    for (i=0; i<myinfo->numrelays; i++)
        memcpy(&myinfo->relaybits[i],&myinfo->relays[i].ipbits,sizeof(myinfo->relaybits[i]));
    revsort32(&myinfo->relaybits[0],myinfo->numrelays,sizeof(myinfo->relaybits[0]));
    for (i=0; i<myinfo->numrelays; i++)
    {
        char ipaddr[64];
        expand_ipbits(ipaddr,myinfo->relaybits[i]);
        printf("%s ",ipaddr);
    }
    printf("sorted\n");
    return(clonestr("{\"result\":\"relay added\"}"));
}

char *basilisk_respond_relays(struct supernet_info *myinfo,char *CMD,void *_addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    bits256 txhash2; uint32_t ipbits; int32_t i,n,len,siglen; uint8_t pubkey33[65],sig[128]; char *sigstr = 0,*retstr,pubstr[128];
    if ( (sigstr= jstr(valsobj,"sig")) != 0 )
    {
        siglen = (int32_t)strlen(sigstr) >> 1;
        if ( siglen < sizeof(sig) )
        {
            decode_hex(sig,siglen,sigstr);
            vcalc_sha256(0,txhash2.bytes,data,datalen);
            memset(pubkey33,0,33);
            if ( bitcoin_recoververify(myinfo->ctx,"BTCD",sig,txhash2,pubkey33) == 0 )
            {
                // compare with existing
                init_hexbytes_noT(pubstr,pubkey33,33);
                printf(" verified relay data siglen.%d pub33.%s\n",siglen,pubstr);
                if ( (retstr= basilisk_addrelay_info(myinfo,pubkey33,(uint32_t)calc_ipbits(remoteaddr),hash)) != 0 )
                    free(retstr);
                n = (int32_t)(datalen / sizeof(uint32_t));
                for (i=len=0; i<n; i++)
                {
                    len += iguana_rwnum(0,(void *)&data[len],sizeof(uint32_t),&ipbits);
                    //printf("(%d %x) ",i,ipbits);
                    if ( (retstr= basilisk_addrelay_info(myinfo,0,ipbits,GENESIS_PUBKEY)) != 0 )
                        free(retstr);
                }
            } else printf("error relay data sig.%d didnt verify\n",siglen);
        }
    }
    return(clonestr("{\"result\":\"processed relays\"}"));
}

int32_t basilisk_relays_send(struct supernet_info *myinfo,struct iguana_peer *addr)
{
    int32_t i,siglen,len = 0; char strbuf[512]; bits256 txhash2; uint8_t sig[128],serialized[sizeof(myinfo->relaybits)]; cJSON *vals; bits256 hash; char *retstr,hexstr[sizeof(myinfo->relaybits)*2 + 1];
    //printf("skip sending relays\n");
    if ( 0 && myinfo != 0 )
    {
        vals = cJSON_CreateObject();
        hash = myinfo->myaddr.persistent;
        for (i=0; i<myinfo->numrelays; i++)
            len += iguana_rwnum(1,&serialized[len],sizeof(uint32_t),&myinfo->relaybits[i]);
        init_hexbytes_noT(hexstr,serialized,len);
        //printf("send relays.(%s)\n",hexstr);
        vcalc_sha256(0,txhash2.bytes,serialized,len);
        if ( 0 && bits256_nonz(myinfo->persistent_priv) != 0 && (siglen= bitcoin_sign(myinfo->ctx,"BTCD",sig,txhash2,myinfo->persistent_priv,1)) > 0 )
        {
            init_hexbytes_noT(strbuf,sig,siglen);
            jaddstr(vals,"sig",strbuf);
        }
        if ( (retstr= basilisk_standardservice("RLY",myinfo,hash,vals,hexstr,0)) != 0 )
            free(retstr);
        free_json(vals);
        return(0);
    } else return(-1);
}

/*char *basilisk_respond_relays(struct supernet_info *myinfo,char *CMD,void *_addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    uint32_t *ipbits = (uint32_t *)data; int32_t num,i,j,n = datalen >> 2;
    for (i=num=0; i<n; i++)
    {
        for (j=0; j<myinfo->numrelays; j++)
            if ( ipbits[i] == myinfo->relays[j].ipbits )
                break;
        if ( j == myinfo->numrelays )
        {
            num++;
            printf("i.%d j.%d ensure new relay.(%s)\n",i,j,remoteaddr);
            basilisk_ensurerelay(iguana_coinfind("BTCD"),ipbits[i]);
        }
    }
    if ( num == 0 )
        return(clonestr("{\"result\":\"no new relays found\"}"));
    else return(clonestr("{\"result\":\"relay added\"}"));
}*/

char *basilisk_respond_goodbye(struct supernet_info *myinfo,char *CMD,void *_addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    struct iguana_peer *addr = _addr;
    printf("(%s) sends goodbye\n",remoteaddr);
    addr->dead = (uint32_t)time(NULL);
    addr->rank = 0;
    return(0);
}

void basilisk_request_goodbye(struct supernet_info *myinfo)
{
    cJSON *valsobj = cJSON_CreateObject();
    jaddnum(valsobj,"timeout",-1);
    basilisk_requestservice(myinfo,"BYE",0,valsobj,GENESIS_PUBKEY,0,0,0);
    free_json(valsobj);
}

char *basilisk_respond_instantdex(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    printf("from.(%s) DEX.(%s) datalen.%d\n",remoteaddr,jprint(valsobj,0),datalen);
    instantdex_quotep2p(myinfo,0,addr,data,datalen);
    return(retstr);
}

char *basilisk_respond_dispatch(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_addrelay(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *ipaddr,*retstr=0;
    if ( (ipaddr= jstr(valsobj,"ipaddr")) != 0 )
        retstr = basilisk_addrelay_info(myinfo,0,(uint32_t)calc_ipbits(ipaddr),jbits256(valsobj,"pubkey"));
    else retstr = clonestr("{\"error\":\"need rmd160, address and ipaddr\"}");
    return(retstr);
}

char *basilisk_respond_forward(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_mailbox(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_VPNcreate(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_VPNjoin(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_VPNlogout(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_VPNbroadcast(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_VPNreceive(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_VPNmessage(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_rawtx(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_value(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

char *basilisk_respond_balances(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash,int32_t from_basilisk)
{
    char *retstr=0;
    return(retstr);
}

#include "../includes/iguana_apidefs.h"
#include "../includes/iguana_apideclares.h"

HASH_ARRAY_STRING(basilisk,balances,hash,vals,hexstr)
{
    return(basilisk_standardservice("BAL",myinfo,hash,vals,hexstr,1));
    //return(basilisk_standardcmd(myinfo,"BAL",activecoin,remoteaddr,basilisktag,vals,coin->basilisk_balances,coin->basilisk_balancesmetric));
}

HASH_ARRAY_STRING(basilisk,value,hash,vals,hexstr)
{
    return(basilisk_standardservice("VAL",myinfo,hash,vals,hexstr,1));
    //return(basilisk_standardcmd(myinfo,"VAL",activecoin,remoteaddr,basilisktag,vals,coin->basilisk_value,coin->basilisk_valuemetric));
}

/*char *basilisk_checkrawtx(int32_t *timeoutmillisp,uint32_t *basilisktagp,char *symbol,cJSON *vals)
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
}*/

HASH_ARRAY_STRING(basilisk,rawtx,hash,vals,hexstr)
{
    char *retstr=0,*symbol; uint32_t basilisktag; struct basilisk_item *ptr,Lptr; int32_t timeoutmillis;
    if ( (symbol= jstr(vals,"symbol")) != 0 || (symbol= jstr(vals,"coin")) != 0 )
    {
        if ( (coin= iguana_coinfind(symbol)) != 0 )
        {
            basilisktag = juint(vals,"basilisktag");
            if ( juint(vals,"burn") == 0 )
                jaddnum(vals,"burn",0.0001);
            if ( (timeoutmillis= juint(vals,"timeout")) <= 0 )
                timeoutmillis = BASILISK_TIMEOUT;
            if ( (ptr= basilisk_bitcoinrawtx(&Lptr,myinfo,coin,remoteaddr,basilisktag,timeoutmillis,vals)) != 0 )
            {
                retstr = ptr->retstr;
            }
            if ( ptr != &Lptr )
                free(ptr);
        }
    }
    return(retstr);

    /*if ( (retstr= basilisk_checkrawtx(&timeoutmillis,(uint32_t *)&basilisktag,activecoin,vals)) == 0 )
    {
        return(basilisk_standardservice("RAW",myinfo,hash,vals,hexstr,1));
        coin = iguana_coinfind(activecoin);
         if ( coin != 0 && (ptr= basilisk_issuecmd(&Lptr,coin->basilisk_rawtx,coin->basilisk_rawtxmetric,myinfo,remoteaddr,basilisktag,activecoin,timeoutmillis,vals)) != 0 )
         {
         if ( (ptr->numrequired= juint(vals,"numrequired")) == 0 )
         ptr->numrequired = 1;
         //ptr->uniqueflag = 1;
         //ptr->metricdir = -1;
         return(basilisk_waitresponse(myinfo,"RAW",coin->symbol,remoteaddr,&Lptr,vals,ptr));
         } else return(clonestr("{\"error\":\"error issuing basilisk rawtx\"}"));
    } //else return(retstr);*/
}

HASH_ARRAY_STRING(basilisk,addrelay,hash,vals,hexstr)
{
    return(basilisk_standardservice("ADD",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,relays,hash,vals,hexstr)
{
    return(basilisk_standardservice("RLY",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,dispatch,hash,vals,hexstr)
{
    return(basilisk_standardservice("RUN",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,publish,hash,vals,hexstr)
{
    return(basilisk_standardservice("PUB",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,subscribe,hash,vals,hexstr)
{
    return(basilisk_standardservice("SUB",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,forward,hash,vals,hexstr)
{
    return(basilisk_standardservice("HOP",myinfo,hash,vals,hexstr,0));
}

HASH_ARRAY_STRING(basilisk,mailbox,hash,vals,hexstr)
{
    return(basilisk_standardservice("BOX",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNcreate,hash,vals,hexstr)
{
    return(basilisk_standardservice("VPN",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNjoin,hash,vals,hexstr)
{
    return(basilisk_standardservice("ARC",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNmessage,hash,vals,hexstr)
{
    return(basilisk_standardservice("GAB",myinfo,hash,vals,hexstr,0));
}

HASH_ARRAY_STRING(basilisk,VPNbroadcast,hash,vals,hexstr)
{
    return(basilisk_standardservice("SAY",myinfo,hash,vals,hexstr,0));
}

HASH_ARRAY_STRING(basilisk,VPNreceive,hash,vals,hexstr)
{
    return(basilisk_standardservice("EAR",myinfo,hash,vals,hexstr,1));
}

HASH_ARRAY_STRING(basilisk,VPNlogout,hash,vals,hexstr)
{
    return(basilisk_standardservice("END",myinfo,hash,vals,hexstr,0));
}

uint16_t basilisk_portavailable(struct supernet_info *myinfo,uint16_t port)
{
    struct iguana_info *coin,*tmp;
    if ( port < 10000 )
        return(0);
    HASH_ITER(hh,myinfo->allcoins,coin,tmp)
    {
        if ( port == coin->chain->portp2p || port == coin->chain->rpcport )
            return(0);
    }
    return(port);
}

HASH_ARRAY_STRING(basilisk,genesis_opreturn,hash,vals,hexstr)
{
    int32_t len; uint16_t blocktime,port; uint8_t p2shval,wifval; uint8_t serialized[4096],tmp[4]; char hex[8192],*symbol,*name,*destaddr; uint64_t PoSvalue; uint32_t nBits;
    symbol = jstr(vals,"symbol");
    name = jstr(vals,"name");
    destaddr = jstr(vals,"issuer");
    PoSvalue = jdouble(vals,"stake") * SATOSHIDEN;
    if ( (blocktime= juint(vals,"blocktime")) == 0 )
        blocktime = 1;
    p2shval = juint(vals,"p2sh");
    wifval = juint(vals,"wif");
    if ( (port= juint(vals,"port")) == 0 )
        while ( (port= basilisk_portavailable(myinfo,(rand() % 50000) + 10000)) == 0 )
            ;
    if ( jstr(vals,"nBits") != 0 )
    {
        decode_hex((void *)&tmp,sizeof(tmp),jstr(vals,"nBits"));
        ((uint8_t *)&nBits)[0] = tmp[3];
        ((uint8_t *)&nBits)[1] = tmp[2];
        ((uint8_t *)&nBits)[2] = tmp[1];
        ((uint8_t *)&nBits)[3] = tmp[0];
    } else nBits = 0x1e00ffff;
    len = datachain_opreturn_create(serialized,symbol,name,destaddr,PoSvalue,nBits,blocktime,port,p2shval,wifval);
    init_hexbytes_noT(hex,serialized,len);
    return(clonestr(hex));
}

#include "../includes/iguana_apiundefs.h"

