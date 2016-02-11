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

// selftest supports against allpairs list

#include "exchanges777.h"

#define INSTANTDEX_HOPS 3
#define INSTANTDEX_DURATION 60

cJSON *InstantDEX_argjson(char *reference,char *message,char *othercoinaddr,char *otherNXTaddr,int32_t iter,int32_t val,int32_t val2)
{
    cJSON *argjson = cJSON_CreateObject();
    if ( reference != 0 )
        jaddstr(argjson,"refstr",reference);
    if ( message != 0 && message[0] != 0 )
        jaddstr(argjson,"message",message);
    if ( othercoinaddr != 0 && othercoinaddr[0] != 0 )
        jaddstr(argjson,"othercoinaddr",othercoinaddr);
    if ( otherNXTaddr != 0 && otherNXTaddr[0] != 0 )
        jaddstr(argjson,"otherNXTaddr",otherNXTaddr);
    //jaddbits256(argjson,"basetxid",basetxid);
    //jaddbits256(argjson,"reltxid",reltxid);
    if ( iter != 3 )
    {
        if ( val == 0 )
            val = INSTANTDEX_DURATION;
        jaddnum(argjson,"duration",val);
        jaddnum(argjson,"flags",val2);
    }
    else
    {
        if ( val > 0 )
            jaddnum(argjson,"baseheight",val);
        if ( val2 > 0 )
            jaddnum(argjson,"relheight",val2);
    }
    return(argjson);
}

int32_t instantdex_rwdata(int32_t rwflag,uint64_t cmdbits,uint8_t *data,int32_t datalen)
{
    // need to inplace serialize/deserialize here
    return(datalen);
}

struct instantdex_msghdr *instantdex_msgcreate(struct supernet_info *myinfo,struct instantdex_msghdr *msg,int32_t datalen)
{
    bits256 otherpubkey; uint64_t signerbits; uint32_t timestamp; uint8_t buf[sizeof(msg->sig)],*data;
    memset(&msg->sig,0,sizeof(msg->sig));
    datalen += (int32_t)(sizeof(*msg) - sizeof(msg->sig));
    data = (void *)((long)msg + sizeof(msg->sig));
    otherpubkey = acct777_msgpubkey(data,datalen);
    timestamp = (uint32_t)time(NULL);
    acct777_sign(&msg->sig,myinfo->privkey,otherpubkey,timestamp,data,datalen);
    if ( (signerbits= acct777_validate(&msg->sig,acct777_msgprivkey(data,datalen),msg->sig.pubkey)) != 0 )
    {
        //int32_t i;
        //char str[65],str2[65];
        //for (i=0; i<datalen; i++)
        //    printf("%02x",data[i]);
        //printf(">>>>>>>>>>>>>>>> validated [%ld] len.%d (%s + %s)\n",(long)data-(long)msg,datalen,bits256_str(str,acct777_msgprivkey(data,datalen)),bits256_str(str2,msg->sig.pubkey));
        memset(buf,0,sizeof(buf));
        acct777_rwsig(1,buf,&msg->sig);
        memcpy(&msg->sig,buf,sizeof(buf));
        return(msg);
    } else printf("error validating instantdex msg\n");
    return(0);
}

char *instantdex_sendcmd(struct supernet_info *myinfo,cJSON *argjson,char *cmdstr,bits256 desthash,int32_t hops)
{
    char *reqstr,*hexstr,*retstr; uint64_t nxt64bits; int32_t i,datalen;
    bits256 instantdexhash; struct instantdex_msghdr *msg;
    instantdexhash = calc_categoryhashes(0,"InstantDEX",0);
    category_subscribe(myinfo,instantdexhash,GENESIS_PUBKEY);
    jaddstr(argjson,"cmd",cmdstr);
    jaddstr(argjson,"agent","SuperNET");
    jaddstr(argjson,"method","DHT");
    jaddstr(argjson,"handle",myinfo->handle);
    jaddbits256(argjson,"categoryhash",instantdexhash);
    jaddbits256(argjson,"traderpub",myinfo->myaddr.persistent);
    nxt64bits = acct777_nxt64bits(myinfo->myaddr.persistent);
    reqstr = jprint(argjson,0);
    datalen = (int32_t)(strlen(reqstr) + 1);
    msg = calloc(1,sizeof(*msg) + datalen);
    for (i=0; i<sizeof(msg->cmd); i++)
        if ( (msg->cmd[i]= cmdstr[i]) == 0 )
            break;
    memcpy(msg->serialized,reqstr,datalen);
    free(reqstr);
    if ( instantdex_msgcreate(myinfo,msg,datalen) != 0 )
    {
        printf("instantdex send.(%s)\n",cmdstr);
        hexstr = malloc(msg->sig.allocsize*2 + 1);
        init_hexbytes_noT(hexstr,(uint8_t *)msg,msg->sig.allocsize);
        retstr = SuperNET_categorymulticast(myinfo,0,instantdexhash,desthash,hexstr,0,hops,1,argjson,0);
        free_json(argjson);
        free(hexstr);
        free(msg);
        return(retstr);
    }
    else
    {
        free_json(argjson);
        free(msg);
        printf("cant msgcreate datalen.%d\n",datalen);
        return(clonestr("{\"error\":\"couldnt create instantdex message\"}"));
    }
}

int32_t instantdex_updatesources(struct exchange_info *exchange,struct exchange_quote *sortbuf,int32_t n,int32_t max,int32_t ind,int32_t dir,struct exchange_quote *quotes,int32_t numquotes)
{
    int32_t i; struct exchange_quote *quote;
    //printf("instantdex_updatesources update dir.%d numquotes.%d\n",dir,numquotes);
    for (i=0; i<numquotes; i++)
    {
        quote = &quotes[i << 1];
        //printf("n.%d ind.%d i.%d dir.%d price %.8f vol %.8f\n",n,ind,i,dir,quote->price,quote->volume);
        if ( quote->price > SMALLVAL )
        {
            sortbuf[n] = *quote;
            sortbuf[n].val = ind;
            sortbuf[n].exchangebits = exchange->exchangebits;
            //printf("sortbuf[%d] <-\n",n*2);
            if ( ++n >= max )
                break;
        }
    }
    return(n);
}

double instantdex_aveprice(struct supernet_info *myinfo,struct exchange_quote *sortbuf,int32_t max,double *totalvolp,char *base,char *rel,double relvolume,cJSON *argjson)
{
    char *str; double totalvol,pricesum; uint32_t timestamp;
    struct exchange_quote quote; int32_t i,n,dir,num,depth = 100;
    struct exchange_info *exchange; struct exchange_request *req,*active[64];
    timestamp = (uint32_t)time(NULL);
    if ( relvolume < 0. )
        relvolume = -relvolume, dir = -1;
    else dir = 1;
    memset(sortbuf,0,sizeof(*sortbuf) * max);
    if ( base != 0 && rel != 0 && relvolume > SMALLVAL )
    {
        for (i=num=0; i<myinfo->numexchanges && num < sizeof(active)/sizeof(*active); i++)
        {
            if ( (exchange= myinfo->tradingexchanges[i]) != 0 )
            {
                if ( (req= exchanges777_baserelfind(exchange,base,rel,'M')) == 0 )
                {
                    if ( (str= exchanges777_Qprices(exchange,base,rel,30,1,depth,argjson,1,exchange->commission)) != 0 )
                        free(str);
                    req = exchanges777_baserelfind(exchange,base,rel,'M');
                }
                if ( req == 0 )
                {
                    if ( (*exchange->issue.supports)(exchange,base,rel,argjson) != 0 )
                        printf("unexpected null req.(%s %s) %s\n",base,rel,exchange->name);
                }
                else
                {
                    //printf("active.%s\n",exchange->name);
                    active[num++] = req;
                }
            }
        }
        for (i=n=0; i<num; i++)
        {
            if ( dir < 0 && active[i]->numbids > 0 )
                n = instantdex_updatesources(active[i]->exchange,sortbuf,n,max,i,1,active[i]->bidasks,active[i]->numbids);
            else if ( dir > 0 && active[i]->numasks > 0 )
                n = instantdex_updatesources(active[i]->exchange,sortbuf,n,max,i,-1,&active[i]->bidasks[1],active[i]->numasks);
        }
        //printf("dir.%d %s/%s numX.%d n.%d\n",dir,base,rel,num,n);
        if ( dir < 0 )
            revsort64s(&sortbuf[0].satoshis,n,sizeof(*sortbuf));
        else sort64s(&sortbuf[0].satoshis,n,sizeof(*sortbuf));
        for (totalvol=pricesum=i=0; i<n && totalvol < relvolume; i++)
        {
            quote = sortbuf[i];
            //printf("n.%d i.%d price %.8f %.8f %.8f\n",n,i,dstr(sortbuf[i].satoshis),sortbuf[i].price,quote.volume);
            if ( quote.satoshis != 0 )
            {
                pricesum += (quote.price * quote.volume);
                totalvol += quote.volume;
                //printf("i.%d of %d %12.8f vol %.8f %s | aveprice %.8f total vol %.8f\n",i,n,sortbuf[i].price,quote.volume,active[quote.val]->exchange->name,pricesum/totalvol,totalvol);
            }
        }
        if ( totalvol > 0. )
        {
            *totalvolp = totalvol;
            return(pricesum / totalvol);
        }
    }
    *totalvolp = 0;
    return(0);
}

int32_t instantdex_bidaskdir(struct instantdex_accept *ap)
{
    if ( ap->A.myside == 0 && ap->A.acceptdir > 0 ) // base
        return(-1);
    else if ( ap->A.myside == 1 && ap->A.acceptdir < 0 ) // rel
        return(1);
    else return(0);
}

cJSON *instantdex_acceptjson(struct instantdex_accept *ap)
{
    int32_t dir;
    cJSON *item = cJSON_CreateObject();
    jadd64bits(item,"orderid",ap->orderid);
    jadd64bits(item,"offerer",ap->A.offer64);
    if ( ap->dead != 0 )
        jadd64bits(item,"dead",ap->dead);
    if ( (dir= instantdex_bidaskdir(ap)) > 0 )
        jaddstr(item,"type","bid");
    else if ( dir < 0 )
        jaddstr(item,"type","ask");
    else
    {
        jaddstr(item,"type","strange");
        jaddnum(item,"acceptdir",ap->A.acceptdir);
        jaddnum(item,"myside",ap->A.myside);
    }
    jaddstr(item,"base",ap->A.base);
    jaddstr(item,"rel",ap->A.rel);
    jaddnum(item,"timestamp",ap->A.expiration);
    jaddnum(item,"price",dstr(ap->A.price64));
    jaddnum(item,"volume",dstr(ap->A.basevolume64));
    jaddnum(item,"nonce",ap->A.nonce);
    jaddnum(item,"pendingvolume",dstr(ap->pendingvolume64));
    jaddnum(item,"expiresin",ap->A.expiration - time(NULL));
    return(item);
}

struct instantdex_accept *instantdex_acceptablefind(struct supernet_info *myinfo,struct exchange_info *exchange,cJSON *bids,cJSON *asks,uint64_t orderid,char *base,char *rel)
{
    struct instantdex_accept PAD,*ap,*retap = 0; uint32_t now; cJSON *item; char *type;
    now = (uint32_t)time(NULL);
    memset(&PAD,0,sizeof(PAD));
    queue_enqueue("acceptableQ",&exchange->acceptableQ,&PAD.DL,0);
    while ( (ap= queue_dequeue(&exchange->acceptableQ,0)) != 0 && ap != &PAD )
    {
        if ( now < ap->A.expiration && ap->dead == 0 )
        {
            printf("find cmps %d %d %d %d %d %d\n",strcmp(base,"*") == 0,strcmp(base,ap->A.base) == 0,strcmp(rel,"*") == 0,strcmp(rel,ap->A.rel) == 0,orderid == 0,orderid == ap->orderid);
            if ( (strcmp(base,"*") == 0 || strcmp(base,ap->A.base) == 0) && (strcmp(rel,"*") == 0 || strcmp(rel,ap->A.rel) == 0) && (orderid == 0 || orderid == ap->orderid) )
            {
                printf("found match\n");
                retap = ap;
            }
            if ( (item= instantdex_acceptjson(ap)) != 0 )
            {
                printf("item.(%s)\n",jprint(item,0));
                if ( (type= jstr(item,"type")) != 0 )
                {
                    if ( strcmp(type,"bid") == 0 && bids != 0 )
                        jaddi(bids,item);
                    else if ( strcmp(type,"ask") == 0 && asks != 0 )
                        jaddi(asks,item);
                }
            }
            queue_enqueue("acceptableQ",&exchange->acceptableQ,&ap->DL,0);
        } else free(ap);
    }
    return(retap);
}

struct instantdex_accept *instantdex_acceptable(struct supernet_info *myinfo,struct exchange_info *exchange,struct instantdex_accept *A,uint64_t offerbits,double minperc)
{
    struct instantdex_accept PAD,*ap,*retap = 0; uint64_t minvol,bestprice64 = 0;
    uint32_t now; int32_t offerdir;
    now = (uint32_t)time(NULL);
    memset(&PAD,0,sizeof(PAD));
    queue_enqueue("acceptableQ",&exchange->acceptableQ,&PAD.DL,0);
    offerdir = instantdex_bidaskdir(A);
    minvol = A->A.basevolume64 * minperc * .01;
    while ( (ap= queue_dequeue(&exchange->acceptableQ,0)) != 0 && ap != &PAD )
    {
        if ( now < ap->A.expiration && ap->dead == 0 && (offerbits == 0 || offerbits != ap->A.offer64) )
        {
            printf("check offerbits.%llu vs %llu: %d %d %d %d %d %d %d %d\n",(long long)offerbits,(long long)ap->A.offer64,A->A.basevolume64 > 0.,strcmp(A->A.base,"*") == 0 ,strcmp(A->A.base,ap->A.base) == 0, strcmp(A->A.rel,"*") == 0 ,strcmp(A->A.rel,ap->A.rel) == 0,A->A.basevolume64 <= (ap->A.basevolume64 - ap->pendingvolume64),offerdir,instantdex_bidaskdir(ap));
            if ( A->A.basevolume64 > 0. && (strcmp(A->A.base,"*") == 0 || strcmp(A->A.base,ap->A.base) == 0) && (strcmp(A->A.rel,"*") == 0 || strcmp(A->A.rel,ap->A.rel) == 0) && minvol <= (ap->A.basevolume64 - ap->pendingvolume64) && offerdir*instantdex_bidaskdir(ap) < 0 )
            {
                printf("passed first cmp: %d %d %d %d\n",offerdir == 0,A->A.price64 == 0,(offerdir > 0 && ap->A.price64 >= A->A.price64),(offerdir < 0 && ap->A.price64 <= A->A.price64));
                if ( offerdir == 0 || A->A.price64 == 0 || ((offerdir > 0 && ap->A.price64 >= A->A.price64) || (offerdir < 0 && ap->A.price64 <= A->A.price64)) )
                {
                    printf("passed second cmp: offerdir.%d best %.8f ap %.8f\n",offerdir,dstr(bestprice64),dstr(ap->A.price64));
                    if ( bestprice64 == 0 || (offerdir < 0 && ap->A.price64 < bestprice64) || (offerdir > 0 && ap->A.price64 > bestprice64) )
                    {
                        printf("found better price %f vs %f\n",dstr(ap->A.price64),dstr(bestprice64));
                        bestprice64 = ap->A.price64;
                        retap = ap;
                    }
                }
            }
            queue_enqueue("acceptableQ",&exchange->acceptableQ,&ap->DL,0);
        } else free(ap);
    }
    return(retap);
}

// NXTrequest:
// sends NXT assetid, volume and desired
// request:
// other node sends (othercoin, othercoinaddr, otherNXT and reftx that expires well before phasedtx)
// proposal:
// NXT node submits phasedtx that refers to it, but it wont confirm
// approve:
// other node verifies unconfirmed has phasedtx and broadcasts cltv, also to NXT node, releases trigger
// confirm:
// NXT node verifies bitcoin txbytes has proper payment and cashes in with onetimepubkey
// BTC* node approves phased tx with onetimepubkey

bits256 instantdex_acceptset(struct instantdex_accept *ap,char *base,char *rel,int32_t duration,int32_t myside,int32_t acceptdir,double price,double volume,uint64_t offerbits)
{
    bits256 hash;
    memset(ap,0,sizeof(*ap));
    safecopy(ap->A.base,base,sizeof(ap->A.base));
    safecopy(ap->A.rel,rel,sizeof(ap->A.rel));
    OS_randombytes((uint8_t *)&ap->A.nonce,sizeof(ap->A.nonce));
    if ( duration < 1000000000 )
        ap->A.expiration = (uint32_t)time(NULL) + duration;
    else ap->A.expiration = duration;
    ap->A.offer64 = offerbits;
    ap->A.myside = myside;
    ap->A.acceptdir = acceptdir;
    ap->A.price64 = price * SATOSHIDEN;
    ap->A.basevolume64 = volume * SATOSHIDEN;
    vcalc_sha256(0,hash.bytes,(void *)&ap->A,sizeof(ap->A));
    ap->orderid = hash.txid;
    //printf("(%s/%s) acceptdir.%d myside.%d\n",base,rel,acceptdir,myside);
    return(hash);
}

int32_t instantdex_acceptextract(struct instantdex_accept *ap,cJSON *argjson)
{
    char *base,*rel; bits256 hash,traderpub; double price,volume; int32_t baserel,acceptdir;
    memset(ap,0,sizeof(*ap));
    if ( (base= jstr(argjson,"base")) != 0 )
    {
        volume = jdouble(argjson,"volume");
        if ( (rel= jstr(argjson,"rel")) != 0 )
            safecopy(ap->A.rel,rel,sizeof(ap->A.rel));
        if ( (price= jdouble(argjson,"maxprice")) > SMALLVAL )
        {
            baserel = 1;
            acceptdir = -1;
        }
        else if ( (price= jdouble(argjson,"minprice")) > SMALLVAL )
        {
            baserel = 0;
            acceptdir = 1;
        } else return(-1);
        //printf("price %f vol %f baserel.%d acceptdir.%d\n",price,volume,baserel,acceptdir);
        traderpub = jbits256(argjson,"traderpub");
        hash = instantdex_acceptset(ap,base,rel,INSTANTDEX_LOCKTIME*2,baserel,acceptdir,price,volume,traderpub.txid);
    }
    else
    {
        if ( (base= jstr(argjson,"b")) != 0 )
            safecopy(ap->A.base,base,sizeof(ap->A.base));
        if ( (rel= jstr(argjson,"r")) != 0 )
            safecopy(ap->A.rel,rel,sizeof(ap->A.rel));
        ap->A.nonce = juint(argjson,"n");
        ap->A.expiration = juint(argjson,"e");
        ap->A.myside = juint(argjson,"s");
        ap->A.acceptdir = jint(argjson,"d");
        ap->A.offer64 = j64bits(argjson,"o");
        ap->A.price64 = j64bits(argjson,"p");
        ap->A.basevolume64 = j64bits(argjson,"v");
        vcalc_sha256(0,hash.bytes,(void *)&ap->A,sizeof(ap->A));
        ap->orderid = j64bits(argjson,"id");
    }
    if ( hash.txid != ap->orderid )
    {
        int32_t i;
        for (i=0; i<sizeof(*ap); i++)
            printf("%02x ",((uint8_t *)ap)[i]);
        printf("instantdex_acceptextract warning %llu != %llu\n",(long long)hash.txid,(long long)ap->orderid);
        return(-1);
    }
    return(0);
}

cJSON *instantdex_acceptsendjson(struct instantdex_accept *ap)
{
    cJSON *json = cJSON_CreateObject();
    jaddstr(json,"b",ap->A.base);
    jaddstr(json,"r",ap->A.rel);
    jaddnum(json,"n",ap->A.nonce);
    jaddnum(json,"e",ap->A.expiration);
    jaddnum(json,"s",ap->A.myside);
    jaddnum(json,"d",ap->A.acceptdir);
    jadd64bits(json,"p",ap->A.price64);
    jadd64bits(json,"v",ap->A.basevolume64);
    jadd64bits(json,"o",ap->A.offer64);
    jadd64bits(json,"id",ap->orderid);
    return(json);
}

#include "swaps/iguana_BTCswap.c"
#include "swaps/iguana_ALTswap.c"
#include "swaps/iguana_NXTswap.c"
#include "swaps/iguana_PAXswap.c"

char *instantdex_parse(struct supernet_info *myinfo,struct instantdex_msghdr *msg,cJSON *argjson,char *remoteaddr,uint64_t signerbits,uint8_t *data,int32_t datalen)
{
    char cmdstr[16],*orderidstr,*retstr; struct exchange_info *exchange; uint64_t orderid;
    struct instantdex_accept A,*ap = 0; bits256 traderpub;
    exchange = exchanges777_find("bitcoin");
    memset(cmdstr,0,sizeof(cmdstr)), memcpy(cmdstr,msg->cmd,sizeof(msg->cmd));
    if ( argjson != 0 )
    {
        traderpub = jbits256(argjson,"traderpub");
        memset(&A,0,sizeof(A));
        if ( instantdex_acceptextract(&A,argjson) < 0 )
        {
            printf("error setting orderid\n");
            return(clonestr("{\"error\":\"hash txid mismatches orderid\"}"));
        }
        printf("A.orderid.%llu\n",(long long)A.orderid);
        if ( bits256_cmp(traderpub,myinfo->myaddr.persistent) == 0 )
        {
            printf("got my own request.(%s)\n",jprint(argjson,0));
            return(clonestr("{\"result\":\"got my own request\"}"));
        }
        if ( (orderidstr= jstr(argjson,"id")) != 0 )
        {
            orderid = calc_nxt64bits(orderidstr);
            printf("orderid.%llu\n",(long long)orderid);
            if ( orderid != A.orderid )
            {
                printf("mismatched orderid %llu != %llu\n",(long long)orderid,(long long)A.orderid);
                return(clonestr("{\"error\":\"specified orderid mismatches calculated orderid\"}"));
            }
            if ( (ap= instantdex_acceptablefind(myinfo,exchange,0,0,orderid,"*","*")) != 0 )
            {
                printf("found existing trade to match\n");
                A = *ap;
            }
            else if ( strcmp(cmdstr+3,"offer") != 0 )
            {
                printf("cant find existing order.%llu that matches\n",(long long)A.orderid);
                return(clonestr("{\"error\":\"cant find matching order\"}"));
            }
        }
        printf("call (%s/%s) swap baserel.%d acceptdir.%d\n",A.A.base,A.A.rel,A.A.myside,A.A.acceptdir);
        if ( strncmp(cmdstr,"BTC",3) == 0 )
            retstr = instantdex_BTCswap(myinfo,exchange,&A,cmdstr+3,msg,argjson,remoteaddr,signerbits,data,datalen);
        else if ( strncmp(cmdstr,"NXT",3) == 0 )
            retstr = instantdex_NXTswap(myinfo,exchange,&A,cmdstr+3,msg,argjson,remoteaddr,signerbits,data,datalen);
        else if ( strncmp(cmdstr,"ALT",3) == 0 )
            retstr = instantdex_ALTswap(myinfo,exchange,&A,cmdstr+3,msg,argjson,remoteaddr,signerbits,data,datalen);
        else if ( strncmp(cmdstr,"PAX",3) == 0 )
            retstr = instantdex_PAXswap(myinfo,exchanges777_find("PAX"),&A,cmdstr+3,msg,argjson,remoteaddr,signerbits,data,datalen);
        else return(clonestr("{\"error\":\"unrecognized atomic swap family\"}"));
        if ( ap != 0 )
        {
            ap->info = A.info;
            ap->pendingvolume64 = A.pendingvolume64;
        }
        printf("after swap ap.%p (%s)\n",ap,retstr);
        return(retstr);
    }
    return(clonestr("{\"error\":\"request needs argjson\"}"));
}

char *InstantDEX_hexmsg(struct supernet_info *myinfo,void *ptr,int32_t len,char *remoteaddr)
{
    struct instantdex_msghdr *msg = ptr; cJSON *argjson; int32_t i,num,n,datalen,newlen,flag = 0;
    uint8_t *data; struct supernet_info *myinfos[64];
    uint64_t signerbits; uint8_t tmp[sizeof(msg->sig)]; char *retstr = 0; cJSON *retjson,*item;
    acct777_rwsig(0,(void *)&msg->sig,(void *)tmp);
    memcpy(&msg->sig,tmp,sizeof(msg->sig));
    datalen = len  - (int32_t)sizeof(msg->sig);
    data = (void *)((long)msg + sizeof(msg->sig));
    if ( remoteaddr != 0 && remoteaddr[0] == 0 && strcmp("127.0.0.1",remoteaddr) == 0 && ((uint8_t *)msg)[len-1] == 0 && (argjson= cJSON_Parse((char *)msg)) != 0 )
    {
        printf("instantdex_hexmsg RESULT.(%s)\n",jprint(argjson,0));
        retstr = instantdex_parse(myinfo,msg,argjson,0,myinfo->myaddr.nxt64bits,0,0);
        free_json(argjson);
        return(retstr);
    }
    //printf("msg.%p len.%d data.%p datalen.%d crc.%u %s\n",msg,len,data,datalen,calc_crc32(0,(void *)msg,len),bits256_str(str,msg->sig.pubkey));
    //return(0);
    else if ( (signerbits= acct777_validate(&msg->sig,acct777_msgprivkey(data,datalen),msg->sig.pubkey)) != 0 )
    {
        flag++;
        //printf("InstantDEX_hexmsg <<<<<<<<<<<<< sigsize.%ld VALIDATED [%ld] len.%d t%u allocsize.%d (%s) [%d]\n",sizeof(msg->sig),(long)data-(long)msg,datalen,msg->sig.timestamp,msg->sig.allocsize,(char *)msg->serialized,data[datalen-1]);
        if ( data[datalen-1] == 0 && (argjson= cJSON_Parse((char *)msg->serialized)) != 0 )
        {
            
        }
        else
        {
            newlen = (int32_t)(msg->sig.allocsize - sizeof(*msg));
            data = msg->serialized;
            if ( msg->serialized[len - 1] == 0 )
            {
                if ( (argjson= cJSON_Parse((char *)msg->serialized)) != 0 )
                {
                    n = (int32_t)(strlen((char *)msg->serialized) + 1);
                    newlen -= n;
                    if ( n >= 0 )
                        data = &msg->serialized[n];
                    else data = 0;
                }
            }
        }
        if ( data != 0 || argjson != 0 )
        {
            printf("CALL instantdex_parse.(%s)\n",argjson!=0?jprint(argjson,0):"");
            retjson = cJSON_CreateArray();
            if ( (num= SuperNET_MYINFOS(myinfos,sizeof(myinfos)/sizeof(*myinfos))) == 0 )
            {
                myinfos[0] = myinfo;
                num = 1;
            }
            for (i=0; i<num; i++)
            {
                myinfo = myinfos[i];
                //char str[65]; printf("i.%d of %d: %s\n",i,num,bits256_str(str,myinfo->myaddr.persistent));
                if ( (retstr= instantdex_parse(myinfo,msg,argjson,remoteaddr,signerbits,data,newlen)) != 0 )
                {
                    item = cJSON_CreateObject();
                    jaddstr(item,"result",retstr);
                    if ( myinfo->handle[0] != 0 )
                        jaddstr(item,"handle",myinfo->handle);
                    jaddbits256(item,"traderpub",myinfo->myaddr.persistent);
                    jaddi(retjson,item);
                }
            }
            retstr = jprint(retjson,1);
        }
    }
    if ( argjson != 0 )
        free_json(argjson);
    return(retstr);
}

char *instantdex_queueaccept(struct supernet_info *myinfo,struct exchange_info *exchange,char *base,char *rel,double price,double basevolume,int32_t acceptdir,char *mysidestr,int32_t duration,uint64_t txid)
{
    struct instantdex_accept *ap; int32_t myside;
    if ( exchange != 0 )
    {
        ap = calloc(1,sizeof(*ap));
        if ( strcmp(mysidestr,base) == 0 )
            myside = 0;
        else if ( strcmp(mysidestr,rel) == 0 )
            myside = 1;
        else
        {
            myside = -1;
            printf("myside.(%s) != base.%s or rel.%s\n",mysidestr,base,rel);
        }
        instantdex_acceptset(ap,base,rel,duration,myside,acceptdir,price,basevolume,myinfo->myaddr.nxt64bits);
        queue_enqueue("acceptableQ",&exchange->acceptableQ,&ap->DL,0);
        return(jprint(instantdex_acceptjson(ap),1));
    }
    else return(clonestr("{\"error\":\"invalid exchange\"}"));
}

void instantdex_update(struct supernet_info *myinfo)
{
    struct instantdex_msghdr *pm; struct category_msg *m; bits256 instantdexhash; char *str,remote[64];
    instantdexhash = calc_categoryhashes(0,"InstantDEX",0);
    while ( (m= category_gethexmsg(myinfo,instantdexhash,GENESIS_PUBKEY)) != 0 )
    {
        pm = (struct instantdex_msghdr *)m->msg;
        if ( m->remoteipbits != 0 )
            expand_ipbits(remote,m->remoteipbits);
        else remote[0] = 0;
        if ( (str= InstantDEX_hexmsg(myinfo,pm,m->len,remote)) != 0 )
            free(str);
        free(m);
    }
}

#include "../includes/iguana_apidefs.h"

TWO_STRINGS_AND_TWO_DOUBLES(InstantDEX,maxaccept,base,rel,maxprice,basevolume)
{
    myinfo = SuperNET_accountfind(json);
    if ( remoteaddr == 0 )
        return(instantdex_queueaccept(myinfo,exchanges777_find("bitcoin"),base,rel,maxprice,basevolume,-1,rel,INSTANTDEX_OFFERDURATION,myinfo->myaddr.nxt64bits));
    else return(clonestr("{\"error\":\"InstantDEX API request only local usage!\"}"));
}

TWO_STRINGS_AND_TWO_DOUBLES(InstantDEX,minaccept,base,rel,minprice,basevolume)
{
    myinfo = SuperNET_accountfind(json);
    if ( remoteaddr == 0 )
        return(instantdex_queueaccept(myinfo,exchanges777_find("bitcoin"),base,rel,minprice,basevolume,1,base,INSTANTDEX_OFFERDURATION,myinfo->myaddr.nxt64bits));
    else return(clonestr("{\"error\":\"InstantDEX API request only local usage!\"}"));
}

/*TWO_STRINGS_AND_TWO_DOUBLES(InstantDEX,BTCoffer,othercoin,otherassetid,maxprice,othervolume)
{
    if ( remoteaddr == 0 )
        return(instantdex_btcoffer(myinfo,exchanges777_find("bitcoin"),othercoin[0] != 0 ? othercoin : otherassetid,othervolume,maxprice));
    else return(clonestr("{\"error\":\"InstantDEX API request only local usage!\"}"));
}

STRING_AND_TWO_DOUBLES(InstantDEX,ALToffer,basecoin,minprice,basevolume)
{
    int32_t hops = INSTANTDEX_HOPS; cJSON *argjson; char *str; struct instantdex_accept A;
    if ( remoteaddr == 0 )
    {
        if ( iguana_coinfind(basecoin) == 0 )
            return(clonestr("{\"error\":\"InstantDEX basecoin is not active, need to addcoin\"}"));
        instantdex_acceptset(&A,basecoin,"BTC",INSTANTDEX_OFFERDURATION,0,1,minprice,basevolume,myinfo->myaddr.nxt64bits);
        argjson = instantdex_acceptsendjson(&A);
        if ( minprice > 0. )
        {
            if ( (str= InstantDEX_minaccept(IGUANA_CALLARGS,basecoin,"BTC",minprice,basevolume)) != 0 )
                free(str);
        }
        return(instantdex_sendcmd(myinfo,argjson,"ALToffer",myinfo->ipaddr,hops));
    } else return(clonestr("{\"error\":\"InstantDEX API request only local usage!\"}"));
}

STRING_AND_TWO_DOUBLES(InstantDEX,NXToffer,assetid,minprice,basevolume)
{
    int32_t hops = INSTANTDEX_HOPS; cJSON *argjson; char *base,*str; struct instantdex_accept A;
    if ( remoteaddr == 0 )
    {
        if ( assetid == 0 || assetid[0] == 0 || strcmp(assetid,"0") == 0 || strcmp(assetid,"NXT") == 0 || strcmp(assetid,"nxt") == 0 )
            base = "NXT";
        else if ( is_decimalstr(assetid) <= 0 )
            return(clonestr("{\"error\":\"InstantDEX NXToffer illegal assetid\"}"));
        else base = assetid;
        instantdex_acceptset(&A,base,"BTC",INSTANTDEX_OFFERDURATION,0,1,minprice,basevolume,myinfo->myaddr.nxt64bits);
        argjson = instantdex_acceptsendjson(&A);
        if ( minprice > 0. )
        {
            if ( (str= InstantDEX_minaccept(IGUANA_CALLARGS,base,"BTC",minprice,basevolume)) != 0 )
                free(str);
        }
        return(instantdex_sendcmd(myinfo,argjson,"NXToffer",myinfo->ipaddr,hops));
    } else return(clonestr("{\"error\":\"InstantDEX API request only local usage!\"}"));
}
*/
#include "../includes/iguana_apiundefs.h"

