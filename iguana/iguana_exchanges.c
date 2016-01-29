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

#include "exchanges777.h"

#define EXCHANGE777_DONE 1
#define EXCHANGE777_ISPENDING 2
#define EXCHANGE777_REQUEUE 3

struct exchange_request
{
    struct queueitem DL;
    cJSON *argjson; char **retstrp;
    double price,volume,hbla,lastbid,lastask,commission;
    uint64_t orderid; uint32_t timedout;
    int32_t dir,depth,func,numbids,numasks;
    char base[32],rel[32],destaddr[64],invert,allflag,dotrade;
    struct exchange_quote bidasks[];
};

char *Exchange_names[] = { "bitfinex", "btc38", "bitstamp", "btce", "poloniex", "bittrex", "huobi", "coinbase", "okcoin", "lakebtc", "quadriga", "truefx", "ecb", "instaforex", "fxcm", "yahoo" };
struct exchange_info *Exchanges[sizeof(Exchange_names)/sizeof(*Exchange_names)];

void prices777_processprice(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *bidasks,int32_t maxdepth)
{
    
}

cJSON *exchanges777_quotejson(struct exchange_quote *quote,int32_t allflag,double pricesum,double totalvol)
{
    cJSON *json; char str[65];
    if ( allflag != 0 )
    {
        json = cJSON_CreateObject();
        if ( totalvol > SMALLVAL )
            pricesum /= totalvol;
        jaddnum(json,"price",quote->price);
        jaddnum(json,"volume",quote->volume);
        jaddnum(json,"aveprice",pricesum);
        jaddnum(json,"cumulative",totalvol);
        if ( quote->timestamp != 0 )
            jaddstr(json,"time",utc_str(str,quote->timestamp));
        if ( quote->orderid > 0 )
            jadd64bits(json,"orderid",quote->orderid);
        if ( quote->offerNXT > 0 )
            jadd64bits(json,"offerNXT",quote->offerNXT);
        return(json);
    } else return(cJSON_CreateNumber(quote->price));
}

char *exchanges777_orderbook_jsonstr(struct exchange_info *exchange,char *_base,char *_rel,struct exchange_quote *bidasks,int32_t maxdepth,int32_t invert,int32_t allflag)
{
    struct exchange_quote *bid,*ask,A,B; cJSON *json,*bids,*asks;
    double highbid,lowask,price,volume,bidsum,asksum,bidvol,askvol;
    uint32_t timestamp;
    int32_t slot,numbids,numasks,enda,endb; char baserel[64],base[64],rel[64],str[65];
    if ( invert == 0 )
    {
        strcpy(base,_base), strcpy(rel,_rel);
        sprintf(baserel,"%s/%s",base,rel);
    }
    else
    {
        strcpy(base,_rel), strcpy(rel,_base);
        sprintf(baserel,"%s/%s",rel,base);
    }
    json = cJSON_CreateObject(), bids = cJSON_CreateArray(), asks = cJSON_CreateArray();
    highbid = lowask = 0.;
    bidsum = asksum = bidvol = askvol = 0.;
    for (slot=numbids=numasks=enda=endb=0; slot<maxdepth && enda+endb!=2; slot++)
    {
        bid = &bidasks[slot << 1], ask = &bidasks[(slot << 1) + 1];
        if ( endb == 0 && (price= bid->price) > SMALLVAL )
        {
            volume = bid->volume;
            if ( invert == 0 )
            {
                bidsum += (price * volume), bidvol += volume;
                //printf("bid %f %f vol %f, cumulative %f %f\n",bid->price,price,volume,bidsum/bidvol,bidvol);
                jaddi(bids,exchanges777_quotejson(bid,allflag,bidsum,bidvol));
                if ( numbids++ == 0 )
                    highbid = price;
            }
            else
            {
                volume *= price;
                price = 1. / price;
                A = *bid;
                A.price = price, A.volume = volume;
                asksum += (price * volume), askvol += volume;
                jaddi(asks,exchanges777_quotejson(&A,allflag,asksum,askvol));
                if ( numasks++ == 0 )
                    lowask = price;
            }
        } else endb = 1;
        if ( enda == 0 && (price= ask->price) > SMALLVAL )
        {
            volume = ask->volume;
            if ( invert == 0 )
            {
                asksum += (price * volume), askvol += volume;
                jaddi(asks,exchanges777_quotejson(ask,allflag,asksum,askvol));
                if ( numasks++ == 0 )
                    lowask = price;
            }
            else
            {
                volume *= price;
                price = 1. / price;
                B = *ask;
                B.price = price, B.volume = volume;
                bidsum += (price * volume), bidvol += volume;
                jaddi(bids,exchanges777_quotejson(&B,allflag,bidsum,bidvol));
                if ( numbids++ == 0 )
                    highbid = price;
            }
        } else enda = 1;
    }
    jaddstr(json,"exchange",exchange->name);
    jaddnum(json,"inverted",invert);
    jaddstr(json,"base",base);
    if ( rel[0] != 0 )
        jaddstr(json,"rel",rel);
    jadd(json,"bids",bids);
    jadd(json,"asks",asks);
    if ( invert == 0 )
    {
        jaddnum(json,"numbids",numbids);
        jaddnum(json,"numasks",numasks);
        if ( highbid > SMALLVAL )
            jaddnum(json,"highbid",highbid);
        if ( lowask > SMALLVAL )
            jaddnum(json,"lowask",lowask);
    }
    else
    {
        jaddnum(json,"numbids",numasks);
        jaddnum(json,"numasks",numbids);
        if ( lowask > SMALLVAL )
            jaddnum(json,"highbid",1. / lowask);
        if ( highbid > SMALLVAL )
            jaddnum(json,"lowask",1. / highbid);
    }
    timestamp = (uint32_t)time(NULL);
    jaddnum(json,"timestamp",timestamp);
    jaddstr(json,"time",utc_str(str,timestamp));
    jaddnum(json,"maxdepth",maxdepth);
    return(jprint(json,1));
}

void exchanges777_json_quotes(struct exchange_info *exchange,double commission,char *base,char *rel,double *lastbidp,double *lastaskp,double *hblap,struct exchange_quote *bidasks,cJSON *bids,cJSON *asks,int32_t maxdepth,char *pricefield,char *volfield,uint32_t reftimestamp)
{
    int32_t i,slot,n=0,m=0,dir,bidask,slot_ba,numitems,numbids,numasks; uint64_t orderid,offerNXT;
    cJSON *item; struct exchange_quote *quote; uint32_t timestamp; double price,volume,hbla = 0.;
    *lastbidp = *lastaskp = 0.;
    numbids = numasks = 0;
    if ( reftimestamp == 0 )
        reftimestamp = (uint32_t)time(NULL);
    if ( bids != 0 )
    {
        n = cJSON_GetArraySize(bids);
        if ( maxdepth != 0 && n > maxdepth )
            n = maxdepth;
    }
    if ( asks != 0 )
    {
        m = cJSON_GetArraySize(asks);
        if ( maxdepth != 0 && m > maxdepth )
            m = maxdepth;
    }
    for (i=0; i<n || i<m; i++)
    {
        for (bidask=0; bidask<2; bidask++)
        {
            offerNXT = orderid = 0;
            price = volume = 0.;
            dir = (bidask == 0) ? 1 : -1;
            if ( bidask == 0 && i >= n )
                continue;
            else if ( bidask == 1 && i >= m )
                continue;
            //if ( strcmp(prices->exchange,"bter") == 0 && dir < 0 )
            //    slot = ((bidask==0?n:m) - 1) - i;
            //else
                slot = i;
            timestamp = 0;
            item = jitem(bidask==0?bids:asks,slot);
            if ( pricefield != 0 && volfield != 0 )
                price = jdouble(item,pricefield), volume = jdouble(item,volfield);
            else if ( is_cJSON_Array(item) != 0 && (numitems= cJSON_GetArraySize(item)) != 0 ) // big assumptions about order within nested array!
            {
                price = jdouble(jitem(item,0),0), volume = jdouble(jitem(item,1),0);
                if ( strcmp(exchange->name,"kraken") == 0 )
                    timestamp = juint(jitem(item,2),0);
                else orderid = j64bits(jitem(item,2),0);
            }
            else continue;
            if ( price > SMALLVAL && volume > SMALLVAL )
            {
                if ( commission != 0. )
                {
                    //printf("price %f fee %f -> ",price,prices->commission * price);
                    if ( bidask == 0 )
                        price -= commission * price;
                    else price += commission * price;
                    //printf("%f\n",price);
                }
                quote = (bidask == 0) ? &bidasks[numbids<<1] : &bidasks[(numasks<<1) + 1];
                quote->price = price, quote->volume = volume, quote->timestamp = timestamp, quote->orderid = orderid, quote->offerNXT = offerNXT;
                if ( bidask == 0 )
                    slot_ba = (numbids++ << 1);
                else slot_ba = (numasks++ << 1) | 1;
                if ( i == 0 )
                {
                    if ( bidask == 0 )
                        *lastbidp = price;
                    else *lastaskp = price;
                    if ( hbla == 0. )
                        hbla = price;
                    else hbla = 0.5 * (hbla + price);
                }
                printf("%d,%d: %-8s %s %5s/%-5s %13.8f vol %13.8f | i %13.8f vol %13.8f | t.%u\n",numbids,numasks,exchange->name,dir>0?"bid":"ask",base,rel,price,volume,1./price,volume*price,timestamp);
            }
        }
    }
    if ( hbla != 0. )
        *hblap = hbla;
}

double exchanges777_json_orderbook(struct exchange_info *exchange,double commission,char *base,char *rel,struct exchange_quote *bidasks,int32_t maxdepth,cJSON *json,char *resultfield,char *bidfield,char *askfield,char *pricefield,char *volfield)
{
    cJSON *obj = 0,*bidobj=0,*askobj=0; double lastbid,lastask,hbla = 0.; int32_t numasks=0,numbids=0;
    if ( resultfield == 0 )
        obj = json;
    if ( maxdepth == 0 )
        maxdepth = EXCHANGES777_MAXDEPTH;
    if ( resultfield == 0 || (obj= jobj(json,resultfield)) != 0 )
    {
        bidobj = jarray(&numbids,obj,bidfield);
        askobj = jarray(&numasks,obj,askfield);
        if ( bidobj != 0 || askobj != 0 )
        {
            exchanges777_json_quotes(exchange,commission,base,rel,&lastbid,&lastask,&hbla,bidasks,bidobj,askobj,maxdepth,pricefield,volfield,0);
        }
    }
    return(hbla);
}

double exchanges777_standardprices(struct exchange_info *exchange,double commission,char *base,char *rel,char *url,struct exchange_quote *quotes,char *price,char *volume,int32_t maxdepth,char *field)
{
    char *jsonstr; cJSON *json; double hbla = 0.;
    if ( (jsonstr= issue_curl(url)) != 0 )
    {
        //if ( strcmp(exchangestr,"btc38") == 0 )
        printf("(%s) -> (%s)\n",url,jsonstr);
        if ( (json= cJSON_Parse(jsonstr)) != 0 )
        {
            hbla = exchanges777_json_orderbook(exchange,commission,base,rel,quotes,maxdepth,json,field,"bids","asks",price,volume);
            free_json(json);
        }
        free(jsonstr);
    }
    return(hbla);
}

char *exchange_would_submit(char *postreq,char *hdr1,char *hdr2,char *hdr3, char *hdr4)
{
    char *data; cJSON *json;
    json = cJSON_CreateObject();
    jaddstr(json,"post",postreq);
    if ( hdr1[0] != 0 )
        jaddstr(json,"hdr1",hdr1);
    if ( hdr2[0] != 0 )
        jaddstr(json,"hdr2",hdr2);
    if ( hdr3[0] != 0 )
        jaddstr(json,"hdr3",hdr3);
    if ( hdr4[0] != 0 )
        jaddstr(json,"hdr4",hdr4);
    data = jprint(json,1);
    json = 0;
    return(data);
}

uint64_t exchange_nonce(struct exchange_info *exchange)
{
    uint64_t nonce;
    nonce = time(NULL);
    if ( nonce < exchange->lastnonce )
        nonce = exchange->lastnonce + 1;
    exchange->lastnonce = nonce;
    return(nonce);
}

int32_t flip_for_exchange(char *pairstr,char *fmt,char *refstr,int32_t dir,double *pricep,double *volumep,char *base,char *rel)
{
    if ( strcmp(rel,refstr) == 0 )
        sprintf(pairstr,fmt,rel,base);
    else
    {
        if ( strcmp(base,refstr) == 0 )
        {
            sprintf(pairstr,fmt,base,rel);
            dir = -dir;
            *volumep *= *pricep;
            *pricep = (1. / *pricep);
        }
        else sprintf(pairstr,fmt,rel,base);
    }
    return(dir);
}

int32_t flipstr_for_exchange(struct exchange_info *exchange,char *pairstr,char *fmt,int32_t dir,double *pricep,double *volumep,char *_base,char *_rel,cJSON *argjson)
{
    int32_t polarity; char base[64],rel[64];
    strcpy(base,_base), strcpy(rel,_rel);
    tolowercase(base), tolowercase(rel);
    polarity = (*exchange->issue.supports)(exchange,base,rel,argjson);
    if ( dir > 0 )
        sprintf(pairstr,fmt,base,rel);
    else if ( dir < 0 )
    {
        *volumep *= *pricep;
        *pricep = (1. / *pricep);
        sprintf(pairstr,fmt,rel,base);
    }
    return(dir);
}

int32_t cny_flip(char *market,char *coinname,char *base,char *rel,int32_t dir,double *pricep,double *volumep)
{
    char pairstr[512],lbase[32],lrel[32],*refstr=0;
    strcpy(lbase,base), tolowercase(lbase), strcpy(lrel,rel), tolowercase(lrel);
    if ( strcmp(lbase,"cny") == 0 || strcmp(lrel,"cny") == 0 )
    {
        dir = flip_for_exchange(pairstr,"%s_%s","cny",dir,pricep,volumep,lbase,lrel);
        refstr = "cny";
    }
    else if ( strcmp(lbase,"btc") == 0 || strcmp(lrel,"btc") == 0 )
    {
        dir = flip_for_exchange(pairstr,"%s_%s","btc",dir,pricep,volumep,lbase,lrel);
        refstr = "btc";
    }
    if ( market != 0 && coinname != 0 && refstr != 0 )
    {
        strcpy(market,refstr);
        if ( strcmp(lbase,"refstr") != 0 )
            strcpy(coinname,lbase);
        else strcpy(coinname,lrel);
        touppercase(coinname);
    }
    return(dir);
}

char *exchange_extractorderid(int32_t historyflag,char *status,uint64_t quoteid,char *quoteid_field)
{
    cJSON *array,*item,*json; int32_t i,n; uint64_t txid;
    if ( status != 0 )
    {
        if ( (array= cJSON_Parse(status)) != 0 && is_cJSON_Array(array) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                if ( (txid= juint(item,quoteid_field)) == quoteid )
                {
                    json = cJSON_CreateObject();
                    jaddstr(json,"result",historyflag == 0 ? "order still pending" : "order completed");
                    jadd(json,"order",cJSON_Duplicate(item,1));
                    free_json(array);
                    return(jprint(json,1));
                }
            }
        }
        if ( array != 0 )
            free_json(array);
    }
    return(0);
}

int32_t baserel_polarity(char *pairs[][2],int32_t n,char *_base,char *_rel)
{
    int32_t i; char base[32],rel[32];
    strcpy(base,_base), tolowercase(base);
    strcpy(rel,_rel), tolowercase(rel);
    for (i=0; i<n; i++)
    {
        if ( strcmp(pairs[i][0],base) == 0 && strcmp(pairs[i][1],rel) == 0 )
            return(1);
        else if ( strcmp(pairs[i][0],rel) == 0 && strcmp(pairs[i][1],base) == 0 )
            return(-1);
    }
    printf("cant find.(%s/%s) [%s/%s].%d\n",base,rel,pairs[0][0],pairs[0][1],n);
    return(0);
}

// following includes C code directly
#include "exchanges/poloniex.c"
#include "exchanges/bittrex.c"
#include "exchanges/btce.c"
#include "exchanges/bitfinex.c"
#include "exchanges/btc38.c"
#include "exchanges/huobi.c"
#include "exchanges/lakebtc.c"
#include "exchanges/quadriga.c"
#include "exchanges/okcoin.c"
#include "exchanges/coinbase.c"
#include "exchanges/bitstamp.c"

char *exchanges777_process(struct exchange_info *exchange,int32_t *retvalp,struct exchange_request *req)
{
    char *retstr = 0; int32_t dir; uint64_t orderid; double balance; cJSON *balancejson,*retjson;
    *retvalp = EXCHANGE777_DONE;
    switch ( req->func )
    {
        case 'Q':
            memset(req->bidasks,0,req->depth * sizeof(*req->bidasks) * 2);
            (*exchange->issue.price)(exchange,req->base,req->rel,req->bidasks,req->depth,req->commission,req->argjson);
            retstr = exchanges777_orderbook_jsonstr(exchange,req->base,req->rel,req->bidasks,req->depth,req->invert,req->allflag);
            break;
        case 'S':
            dir = (*exchange->issue.supports)(exchange,req->base,req->rel,req->argjson);
            retjson = cJSON_CreateObject();
            jaddnum(retjson,"result",dir);
            retstr = jprint(retjson,1);
            break;
        case 'T':
            orderid = (*exchange->issue.trade)(req->dotrade,&retstr,exchange,req->base,req->rel,req->dir,req->price,req->volume,req->argjson);
            if ( retstr == 0 )
            {
                retjson = cJSON_CreateObject();
                if ( orderid != 0 )
                    jadd64bits(retjson,"result",orderid);
                else jaddstr(retjson,"error","no return value from trade call");
                retstr = jprint(retjson,1);
            }
            break;
        case 'B':
            if ( (balancejson= (*exchange->issue.balances)(exchange,req->argjson)) != 0 )
            {
                retstr = (*exchange->issue.parsebalance)(exchange,&balance,req->base,balancejson);
                free_json(balancejson);
            }
            break;
        case 'P':
            retstr = (*exchange->issue.orderstatus)(exchange,req->orderid,req->argjson);
            break;
        case 'C':
            retstr = (*exchange->issue.cancelorder)(exchange,req->orderid,req->argjson);
            break;
        case 'O':
            retstr = (*exchange->issue.openorders)(exchange,req->argjson);
            break;
        case 'H':
            retstr = (*exchange->issue.tradehistory)(exchange,req->argjson);
            break;
        case 'W':
            retstr = (*exchange->issue.withdraw)(exchange,req->base,req->volume,req->destaddr,req->argjson);
            break;
    }
    return(retstr);
}

void exchanges777_loop(void *ptr)
{
    int32_t flag,retval; struct exchange_request *req; char *retstr; struct exchange_info *exchange = ptr;
    while ( 1 )
    {
        flag = retval = 0;
        retstr = 0;
        if ( (req= queue_dequeue(&exchange->requestQ,0)) != 0 )
        {
            //printf("dequeued %s.%c\n",exchange->name,req->func);
            retstr = exchanges777_process(exchange,&retval,req);
            if ( retval == EXCHANGE777_DONE )
            {
                if ( retstr != 0 )
                {
                    if ( req->retstrp != 0 && req->timedout == 0 )
                        *req->retstrp = retstr;
                    else free(retstr);
                    if ( req->timedout != 0 )
                        printf("timedout.%u req finally finished at %u\n",req->timedout,(uint32_t)time(NULL));
                }
                free(req);
                flag++;
            }
            else
            {
                if ( retstr != 0 )
                    free(retstr);
                if ( retval == EXCHANGE777_ISPENDING )
                    queue_enqueue("Xpending",&exchange->pendingQ[0],&req->DL,0), flag++;
                else if ( retval == EXCHANGE777_REQUEUE )
                    queue_enqueue("requeue",&exchange->requestQ,&req->DL,0);
                else
                {
                    printf("exchanges777_process: illegal retval.%d\n",retval);
                    free(req);
                }
            }
        }
        if ( flag == 0 && time(NULL) > exchange->lastpoll+exchange->pollgap )
        {
            if ( (req= queue_dequeue(&exchange->pricesQ,0)) != 0 )
            {
                if ( req->base[0] != 0 )
                {
                    //printf("check %s pricesQ (%s %s)\n",exchange->name,req->base,req->rel);
                    exchange->lastpoll = (uint32_t)time(NULL);
                    req->hbla = (*exchange->issue.price)(exchange,req->base,req->rel,req->bidasks,req->depth,req->commission,req->argjson);
                    prices777_processprice(exchange,req->base,req->rel,req->bidasks,req->depth);
                }
                queue_enqueue("pricesQ",&exchange->pricesQ,&req->DL,0);
            }
        }
        if ( flag == 0 )
            sleep(exchange->pollgap/2 + 1);
    }
}

char *exchanges777_unmonitor(struct exchange_info *exchange,char *base,char *rel)
{
    struct exchange_request PAD,*req; char *retstr = 0;
    memset(&PAD,0,sizeof(PAD));
    queue_enqueue("pricesQ",&exchange->pricesQ,&PAD.DL,0);
    while ( (req= queue_dequeue(&exchange->pricesQ,0)) != 0 && req != &PAD )
    {
        if ( strcmp(base,req->base) == 0 || strcmp(rel,req->rel) == 0 )
        {
            printf("unmonitor.%s (%s %s)\n",exchange->name,base,rel);
            free(req);
            retstr = clonestr("{\"result\":\"unmonitored\"}");
        } else queue_enqueue("pricesQ",&exchange->pricesQ,&req->DL,0);
    }
    if ( retstr == 0 )
        retstr = clonestr("{\"error\":\"cant find base/rel pair to unmonitor\"}");
    return(retstr);
}

char *exchanges777_submit(struct exchange_info *exchange,char **retstrp,struct exchange_request *req,int32_t func,int32_t maxseconds)
{
    int32_t i;
    req->func = func;
    if ( maxseconds == 0 )
        maxseconds = EXCHANGES777_DEFAULT_TIMEOUT;
    queue_enqueue("exchangeQ",&exchange->requestQ,&req->DL,0);
    for (i=0; i<maxseconds; i++)
    {
        if ( *retstrp != 0 )
            return(*retstrp);
        sleep(1);
    }
    req->timedout = (uint32_t)time(NULL);
    return(clonestr("{\"error\":\"request timed out\"}"));
}

char *exchanges777_Qtrade(struct exchange_info *exchange,char *base,char *rel,int32_t maxseconds,int32_t dotrade,int32_t dir,double price,double volume,cJSON *argjson)
{
    struct exchange_request *req; int32_t polarity; char *retstr = 0;
    if ( base[0] == 0 || rel[0] == 0 || (polarity= (*exchange->issue.supports)(exchange,base,rel,argjson)) == 0 || price < SMALLVAL || volume < SMALLVAL )
        return(clonestr("{\"error\":\"invalid base or rel\"}"));
    req = calloc(1,sizeof(*req));
    req->argjson = argjson; req->retstrp = &retstr;
    safecopy(req->base,base,sizeof(req->base));
    safecopy(req->rel,rel,sizeof(req->rel));
    if ( polarity < 0 )
        dir *= -1, volume *= price, price = 1. / price;
    req->price = price, req->volume = volume, req->dir = dir;
    req->dotrade = dotrade;
    return(exchanges777_submit(exchange,&retstr,req,'T',maxseconds));
}

char *exchanges777_Qprices(struct exchange_info *exchange,char *base,char *rel,int32_t maxseconds,int32_t allfields,int32_t depth,cJSON *argjson,int32_t monitor,double commission)
{
    struct exchange_request *req; char *retstr = 0; int32_t polarity;
    if ( base[0] == 0 || rel[0] == 0 || (polarity= (*exchange->issue.supports)(exchange,base,rel,argjson)) == 0 )
        return(clonestr("{\"error\":\"invalid base or rel\"}"));
    if ( depth <= 0 )
        depth = 1;
    req = calloc(1,sizeof(*req) + sizeof(*req->bidasks)*depth*2);
    req->argjson = argjson; req->retstrp = &retstr;
    safecopy(req->base,base,sizeof(req->base));
    safecopy(req->rel,rel,sizeof(req->rel));
    req->depth = depth, req->allflag = allfields;
    req->invert = (polarity < 0);
    if ( (req->commission= commission) == 0. )
        req->commission = exchange->commission;
    if ( monitor == 0 )
        return(exchanges777_submit(exchange,&retstr,req,'Q',maxseconds));
    else
    {
        req->func = 'Q';
        queue_enqueue("pricesQ",&exchange->pricesQ,&req->DL,0);
        return(clonestr("{\"result\":\"start monitoring\"}"));
    }
}

char *exchanges777_Qrequest(struct exchange_info *exchange,int32_t func,char *base,char *rel,int32_t maxseconds,uint64_t orderid,char *destaddr,double amount,cJSON *argjson)
{
    struct exchange_request *req; char *retstr = 0;
    req = calloc(1,sizeof(*req));
    req->volume = amount;
    safecopy(req->destaddr,destaddr,sizeof(req->destaddr));
    safecopy(req->base,base,sizeof(req->base));
    safecopy(req->rel,rel,sizeof(req->rel));
    req->orderid = orderid;
    return(exchanges777_submit(exchange,&retstr,req,func,maxseconds));
}

int32_t exchanges777_id(char *exchangestr)
{
    int32_t i;
    for (i=0; i<sizeof(Exchange_names)/sizeof(*Exchange_names); i++)
    {
        if ( strcmp(exchangestr,Exchange_names[i]) == 0 )
            return(i);
    }
    return(-1);
}

struct exchange_info *exchanges777_find(char *exchangestr)
{
    int32_t exchangeid;
    if ( (exchangeid= exchanges777_id(exchangestr)) >= 0 )
        return(Exchanges[exchangeid]);
    return(0);
}

struct exchange_info *exchange_create(char *exchangestr,cJSON *argjson)
{
    struct exchange_funcs funcs[] =
    {
        {"truefx", 0 }, {"ecb", 0 }, {"instaforex", 0 }, {"fxcm", 0 }, {"yahoo", 0 },
        poloniex_funcs, bittrex_funcs, btce_funcs, bitfinex_funcs, btc38_funcs,
        huobi_funcs, lakebtc_funcs, quadriga_funcs, okcoin_funcs, coinbase_funcs, bitstamp_funcs
    };
    char *key,*secret,*userid,*tradepassword; struct exchange_info *exchange; int32_t i,exchangeid;
    if ( (exchangeid= exchanges777_id(exchangestr)) < 0 )
    {
        printf("exchange_create: cant find.(%s)\n",exchangestr);
        return(0);
    }
    for (i=0; i<sizeof(funcs)/sizeof(*funcs); i++)
    {
        if ( strcmp(exchangestr,funcs[i].name) == 0 )
        {
            break;
        }
    }
    if ( i == sizeof(funcs)/sizeof(*funcs) )
    {
        printf("cant find exchange.(%s)\n",exchangestr);
        return(0);
    }
    exchange = calloc(1,sizeof(*exchange));
    exchange->issue = funcs[i];
    iguana_initQ(&exchange->pricesQ,"prices");
    iguana_initQ(&exchange->requestQ,"request");
    iguana_initQ(&exchange->pendingQ[0],"pending0");
    iguana_initQ(&exchange->pendingQ[1],"pending1");
    exchange->exchangeid = exchangeid;
    safecopy(exchange->name,exchangestr,sizeof(exchange->name));
    if ( (exchange->pollgap= juint(argjson,"pollgap")) < EXCHANGES777_MINPOLLGAP )
        exchange->pollgap = EXCHANGES777_MINPOLLGAP;
    if ( (key= jstr(argjson,"apikey")) != 0 || (key= jstr(argjson,"key")) != 0 )
        safecopy(exchange->apikey,key,sizeof(exchange->apikey));
    if ( (secret= jstr(argjson,"apisecret")) != 0 || (secret= jstr(argjson,"secret")) != 0 )
        safecopy(exchange->apisecret,secret,sizeof(exchange->apisecret));
    if ( (userid= jstr(argjson,"userid")) != 0 )
        safecopy(exchange->userid,userid,sizeof(exchange->userid));
    if ( (tradepassword= jstr(argjson,"tradepassword")) != 0 )
        safecopy(exchange->tradepassword,tradepassword,sizeof(exchange->tradepassword));
    if ( (exchange->commission= jdouble(argjson,"commission")) > 0. )
        exchange->commission *= .01;
    printf("ADDEXCHANGE.(%s) [%s, %s, %s] commission %.3f%%\n",exchangestr,exchange->apikey,exchange->userid,exchange->apisecret,exchange->commission * 100.);
    Exchanges[exchangeid] = exchange;
    return(exchange);
}

struct exchange_info *exchanges777_info(char *exchangestr,int32_t sleepflag,cJSON *json,char *remoteaddr)
{
    struct exchange_info *exchange;
    if ( remoteaddr != 0 )
        return(0);
    if ( (exchange= exchanges777_find(exchangestr)) == 0 )
    {
        if ( (exchange= exchange_create(exchangestr,json)) != 0 )
        {
            iguana_launch(iguana_coinadd("BTCD"),"exchangeloop",(void *)exchanges777_loop,exchange,IGUANA_EXCHANGETHREAD);
            if ( sleepflag > 0 )
                sleep(sleepflag);
        }
    }
    return(exchange);
}

void exchanges777_init(int32_t sleepflag)
{
    int32_t i; cJSON *argjson; bits256 instantdexhash;
    if ( 0 )
    {
        argjson = cJSON_CreateObject();
        for (i=0; i<sizeof(Exchange_names)/sizeof(*Exchange_names); i++)
            exchanges777_info(Exchange_names[i],sleepflag,argjson,0);
        free_json(argjson);
    }
    instantdexhash = calc_categoryhashes(0,"InstantDEX",0);
    category_subscribe(SuperNET_MYINFO(0),instantdexhash,GENESIS_PUBKEY);
    category_processfunc(instantdexhash,InstantDEX_hexmsg);
}

#include "../includes/iguana_apidefs.h"

THREE_STRINGS_AND_THREE_INTS(InstantDEX,orderbook,exchange,base,rel,depth,allfields,invert)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
       return(exchanges777_Qprices(ptr,base,rel,juint(json,"maxseconds"),allfields,depth,json,0,ptr->commission));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

THREE_STRINGS_AND_THREE_DOUBLES(InstantDEX,buy,exchange,base,rel,price,volume,dotrade)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qtrade(ptr,base,rel,juint(json,"maxseconds"),dotrade,1,price,volume,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

THREE_STRINGS_AND_THREE_DOUBLES(InstantDEX,sell,exchange,base,rel,price,volume,dotrade)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qtrade(ptr,base,rel,juint(json,"maxseconds"),dotrade,-1,price,volume,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

THREE_STRINGS_AND_DOUBLE(InstantDEX,withdraw,exchange,base,destaddr,amount)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'W',base,0,juint(json,"maxseconds"),0,destaddr,amount,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

THREE_STRINGS(InstantDEX,supports,exchange,base,rel)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'S',base,rel,juint(json,"maxseconds"),0,0,0,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

TWO_STRINGS(InstantDEX,balance,exchange,base)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'B',base,0,juint(json,"maxseconds"),0,0,0,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

TWO_STRINGS(InstantDEX,orderstatus,exchange,orderid)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'P',0,0,juint(json,"maxseconds"),calc_nxt64bits(orderid),0,0,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

TWO_STRINGS(InstantDEX,cancelorder,exchange,orderid)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'C',0,0,juint(json,"maxseconds"),calc_nxt64bits(orderid),0,0,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

STRING_ARG(InstantDEX,openorders,exchange)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'O',0,0,juint(json,"maxseconds"),0,0,0,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

STRING_ARG(InstantDEX,tradehistory,exchange)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
        return(exchanges777_Qrequest(ptr,'H',0,0,juint(json,"maxseconds"),0,0,0,json));
    else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

THREE_STRINGS(InstantDEX,apikeypair,exchange,apikey,apisecret)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
    {
        if ( apikey != 0 && apikey[0] != 0 && apisecret != 0 && apisecret[0] != 0 )
        {
            safecopy(ptr->apikey,apikey,sizeof(ptr->apikey));
            safecopy(ptr->apisecret,apisecret,sizeof(ptr->apisecret));
            return(clonestr("{\"result\":\"set apikey and apisecret\"}"));
        } else return(clonestr("{\"error\":\"need both userid and password\"}"));
    } else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

THREE_STRINGS(InstantDEX,setuserid,exchange,userid,tradepassword)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
    {
        safecopy(ptr->userid,userid,sizeof(ptr->userid));
        safecopy(ptr->tradepassword,tradepassword,sizeof(ptr->tradepassword));
        return(clonestr("{\"result\":\"set userid and/or tradepassword\"}"));
    } else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}

STRING_AND_INT(InstantDEX,pollgap,exchange,pollgap)
{
    struct exchange_info *ptr;
    if ( (ptr= exchanges777_info(exchange,1,json,remoteaddr)) != 0 )
    {
        ptr->pollgap = pollgap;
        return(clonestr("{\"result\":\"set pollgap\"}"));
    } else return(clonestr("{\"error\":\"cant find or create exchange\"}"));
}
#include "../includes/iguana_apiundefs.h"
