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

#include "iguana777.h"
#include "../includes/iguana_apidefs.h"

STRING_ARG(iguana,validate,activecoin)
{
    int32_t i,total,validated; struct iguana_bundle *bp; cJSON *retjson;
    if ( (coin= iguana_coinfind(activecoin)) != 0 )
    {
        for (i=total=validated=0; i<coin->bundlescount; i++)
            if ( (bp= coin->bundles[i]) != 0 )
            {
                validated += iguana_bundlevalidate(coin,bp,1);
                total += bp->n;
            }
        retjson = cJSON_CreateObject();
        jaddstr(retjson,"result","validation run");
        jaddstr(retjson,"coin",coin->symbol);
        jaddnum(retjson,"validated",validated);
        jaddnum(retjson,"total",total);
        jaddnum(retjson,"bundles",coin->bundlescount);
        jaddnum(retjson,"accuracy",(double)validated/total);
        return(jprint(retjson,1));
    } else return(clonestr("{\"error\":\"no active coin\"}"));
}

STRING_ARG(iguana,removecoin,activecoin)
{
    struct iguana_bundle *bp; int32_t i,height; char fname[1024];
    if ( (coin= iguana_coinfind(activecoin)) != 0 )
    {
        coin->active = 0;
        coin->started = 0;
        for (i=0; i<IGUANA_MAXPEERS; i++)
        {
            sprintf(fname,"%s/%s/vouts/%04d.vouts",GLOBAL_DBDIR,coin->symbol,i), OS_removefile(fname,0);
            sprintf(fname,"%s/%s/%04d.vins",coin->VALIDATEDIR,coin->symbol,i), OS_removefile(fname,0);
        }
        sprintf(fname,"%s/%s/vouts/*",GLOBAL_DBDIR,coin->symbol), OS_removefile(fname,0);
        sprintf(fname,"%s/%s/*",coin->VALIDATEDIR,coin->symbol), OS_removefile(fname,0);
        for (i=0; i<coin->bundlescount; i++)
        {
            sprintf(fname,"%s/%s/balancecrc.%d",GLOBAL_DBDIR,coin->symbol,i), OS_removefile(fname,0);
            if ( (bp= coin->bundles[i]) != 0 )
            {
                iguana_bundlepurgefiles(coin,bp);
                iguana_bundleremove(coin,bp->hdrsi,1);
            }
        }
        for (height=0; height<coin->longestchain; height+=IGUANA_SUBDIRDIVISOR)
        {
            sprintf(fname,"%s/%s/%d",GLOBAL_DBDIR,coin->symbol,height/IGUANA_SUBDIRDIVISOR);
            OS_remove_directory(fname);
        }
        sprintf(fname,"%s/%s/*",GLOBAL_DBDIR,coin->symbol), OS_remove_directory(fname);
    }
    return(clonestr("{\"error\":\"no active coin\"}"));
}

char *iguana_APIrequest(struct iguana_info *coin,bits256 blockhash,bits256 txid,int32_t seconds)
{
    int32_t i,len; char *retstr = 0; uint8_t serialized[1024]; char str[65];
    coin->APIblockhash = blockhash;
    coin->APItxid = txid;
    printf("request block.(%s) txid.%llx\n",bits256_str(str,blockhash),(long long)txid.txid);
    if ( (len= iguana_getdata(coin,serialized,MSG_BLOCK,&blockhash,1)) > 0 )
    {
        for (i=0; i<seconds; i++)
        {
            if ( i == 0 )
                iguana_send(coin,0,serialized,len);
            if ( coin->APIblockstr != 0 )
            {
                retstr = coin->APIblockstr;
                coin->APIblockstr = 0;
                memset(&coin->APIblockhash,0,sizeof(coin->APIblockhash));
                memset(&coin->APItxid,0,sizeof(coin->APItxid));
                return(retstr);
            }
            sleep(1);
        }
    }
    return(0);
}

INT_ARG(bitcoinrpc,getblockhash,height)
{
    cJSON *retjson = cJSON_CreateObject();
    jaddbits256(retjson,"result",iguana_blockhash(coin,height));
    return(jprint(retjson,1));
}

HASH_AND_TWOINTS(bitcoinrpc,getblock,blockhash,verbose,remoteonly)
{
    char *blockstr,*datastr; struct iguana_msgblock msg; struct iguana_block *block; cJSON *retjson; bits256 txid; int32_t len;
    retjson = cJSON_CreateObject();
    memset(&msg,0,sizeof(msg));
    if ( remoteonly == 0 && (block= iguana_blockfind("getblockRPC",coin,blockhash)) != 0 )
    {
        if ( verbose != 0 )
            return(jprint(iguana_blockjson(coin,block,1),1));
        else
        {
            if ( (len= iguana_peerblockrequest(coin,coin->blockspace,sizeof(coin->blockspace),0,blockhash,0)) > 0 )
            {
                datastr = malloc(len*2 + 1);
                init_hexbytes_noT(datastr,coin->blockspace,len);
                jaddstr(retjson,"result",datastr);
                free(datastr);
                return(jprint(retjson,1));
            }
            jaddstr(retjson,"error","error getting rawblock");
        }
    }
    else if ( coin->APIblockstr != 0 )
        jaddstr(retjson,"error","already have pending request");
    else
    {
        memset(txid.bytes,0,sizeof(txid));
        if ( (blockstr= iguana_APIrequest(coin,blockhash,txid,5)) != 0 )
        {
            jaddstr(retjson,"result",blockstr);
            free(blockstr);
        } else jaddstr(retjson,"error","cant find blockhash");
    }
    return(jprint(retjson,1));
}

HASH_AND_INT(bitcoinrpc,getrawtransaction,txid,verbose)
{
    struct iguana_txid *tx,T; char *txbytes; bits256 checktxid; int32_t len,height; cJSON *retjson;
    if ( (tx= iguana_txidfind(coin,&height,&T,txid,coin->bundlescount-1)) != 0 )
    {
        retjson = cJSON_CreateObject();
        if ( (len= iguana_ramtxbytes(coin,coin->blockspace,sizeof(coin->blockspace),&checktxid,tx,height,0,0,0)) > 0 )
        {
            txbytes = calloc(1,len*2+1);
            init_hexbytes_noT(txbytes,coin->blockspace,len);
            jaddstr(retjson,"result",txbytes);
            //printf("txbytes.(%s)\n",txbytes);
            free(txbytes);
            return(jprint(retjson,1));
        }
        else if ( height >= 0 )
        {
            if ( coin->APIblockstr != 0 )
                jaddstr(retjson,"error","already have pending request");
            else
            {
                int32_t datalen; uint8_t *data; char *blockstr; bits256 blockhash;
                blockhash = iguana_blockhash(coin,height);
                if ( (blockstr= iguana_APIrequest(coin,blockhash,txid,2)) != 0 )
                {
                    datalen = (int32_t)(strlen(blockstr) >> 1);
                    data = malloc(datalen);
                    decode_hex(data,datalen,blockstr);
                    if ( (txbytes= iguana_txscan(coin,verbose != 0 ? retjson : 0,data,datalen,txid)) != 0 )
                    {
                        jaddstr(retjson,"result",txbytes);
                        jaddbits256(retjson,"blockhash",blockhash);
                        jaddnum(retjson,"height",height);
                        free(txbytes);
                    } else jaddstr(retjson,"error","cant find txid in block");
                    free(blockstr);
                    free(data);
                } else jaddstr(retjson,"error","cant find blockhash");
                return(jprint(retjson,1));
            }
        } else printf("height.%d\n",height);
    }
    return(clonestr("{\"error\":\"cant find txid\"}"));
}

STRING_ARG(bitcoinrpc,decoderawtransaction,rawtx)
{
    cJSON *txobj = 0; bits256 txid;
    if ( rawtx != 0 && rawtx[0] != 0 )
    {
        if ( (strlen(rawtx) & 1) != 0 )
            return(clonestr("{\"error\":\"rawtx hex has odd length\"}"));
        txobj = bitcoin_hex2json(coin,&txid,0,rawtx);
        char str[65]; printf("got txid.(%s)\n",bits256_str(str,txid));
    }
    if ( txobj == 0 )
        txobj = cJSON_CreateObject();
    return(jprint(txobj,1));
}

HASH_ARG(bitcoinrpc,gettransaction,txid)
{
    return(bitcoinrpc_getrawtransaction(IGUANA_CALLARGS,txid,1));
}

ZERO_ARGS(bitcoinrpc,getbestblockhash)
{
    cJSON *retjson = cJSON_CreateObject();
    char str[65]; jaddstr(retjson,"result",bits256_str(str,coin->blocks.hwmchain.RO.hash2));
    return(jprint(retjson,1));
}

ZERO_ARGS(bitcoinrpc,getblockcount)
{
    cJSON *retjson = cJSON_CreateObject();
    jaddnum(retjson,"result",coin->blocks.hwmchain.height);
    return(jprint(retjson,1));
}

ZERO_ARGS(bitcoinrpc,makekeypair)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

STRING_ARG(bitcoinrpc,validatepubkey,pubkey)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

HASH_AND_TWOINTS(bitcoinrpc,listsinceblock,blockhash,target,flag)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

STRING_ARG(bitcoinrpc,decodescript,script)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

STRING_ARG(bitcoinrpc,vanitygen,vanity)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

TWO_STRINGS(bitcoinrpc,signmessage,address,message)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

THREE_STRINGS(bitcoinrpc,verifymessage,address,sig,message)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

// tx
TWO_ARRAYS(bitcoinrpc,createrawtransaction,vins,vouts)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

STRING_AND_TWOARRAYS(bitcoinrpc,signrawtransaction,rawtx,vins,privkeys)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

STRING_AND_INT(bitcoinrpc,sendrawtransaction,rawtx,allowhighfees)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

// unspents
ZERO_ARGS(bitcoinrpc,gettxoutsetinfo)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

INT_AND_ARRAY(bitcoinrpc,lockunspent,flag,array)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

ZERO_ARGS(bitcoinrpc,listlockunspent)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

HASH_AND_TWOINTS(bitcoinrpc,gettxout,txid,vout,mempool)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

TWOINTS_AND_ARRAY(bitcoinrpc,listunspent,minconf,maxconf,array)
{
    int32_t numrmds; uint8_t *rmdarray; cJSON *retjson = cJSON_CreateArray();
    if ( minconf == 0 )
        minconf = 1;
    if ( maxconf == 0 )
        maxconf = 9999999;
    rmdarray = iguana_rmdarray(coin,&numrmds,array,0);
    iguana_unspents(myinfo,coin,retjson,minconf,maxconf,rmdarray,numrmds);
    if ( rmdarray != 0 )
        free(rmdarray);
    return(jprint(retjson,1));
}

STRING_AND_INT(bitcoinrpc,getreceivedbyaddress,address,minconf)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}


// single address/account funcs
ZERO_ARGS(bitcoinrpc,getrawchangeaddress)
{
    cJSON *retjson = cJSON_CreateObject();
    return(jprint(retjson,1));
}

HASH_AND_STRING(bitcoinrpc,verifytx,txid,txbytes)
{
    cJSON *retjson;
    retjson = bitcoin_txtest(coin,txbytes,txid);
    //printf("verifytx.(%s) %p\n",jprint(retjson,0),retjson);
    return(jprint(retjson,1));
}

STRING_AND_INT(iguana,bundleaddresses,activecoin,height)
{
    struct iguana_info *ptr;
    if ( (ptr= iguana_coinfind(activecoin)) != 0 )
        return(iguana_bundleaddrs(ptr,height / coin->chain->bundlesize));
    else return(clonestr("{\"error\":\"activecoin is not active\"}"));
}

STRING_AND_INT(iguana,bundlehashes,activecoin,height)
{
    struct iguana_info *ptr; struct iguana_bundle *bp; int32_t i,hdrsi; cJSON *retjson,*array; struct iguana_ramchaindata *rdata;
    if ( (ptr= iguana_coinfind(activecoin)) != 0 )
    {
        hdrsi = height / coin->chain->bundlesize;
        if ( hdrsi < coin->bundlescount && hdrsi >= 0 && (bp= coin->bundles[hdrsi]) != 0 )
        {
            if ( (rdata= bp->ramchain.H.data) != 0 )
            {
                array = cJSON_CreateArray();
                for (i=0; i<IGUANA_NUMLHASHES; i++)
                    jaddinum(array,rdata->lhashes[i].txid);
                retjson = cJSON_CreateObject();
                jaddstr(retjson,"result","success");
                jaddbits256(retjson,"sha256",rdata->sha256);
                jadd(retjson,"bundlehashes",array);
                return(jprint(retjson,1));
            } else return(clonestr("{\"error\":\"ramchain not there\"}"));
        } else return(clonestr("{\"error\":\"height is too big\"}"));
    } else return(clonestr("{\"error\":\"activecoin is not active\"}"));
}

#undef IGUANA_ARGS
#include "../includes/iguana_apiundefs.h"

