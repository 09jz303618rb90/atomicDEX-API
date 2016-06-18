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

// included from gecko.c

int32_t basilisk_respond_geckogetheaders(struct supernet_info *myinfo,struct iguana_info *virt,uint8_t *serialized,int32_t maxsize,cJSON *valsobj,bits256 hash2)
{
    int32_t i,n,num,height,len=0; struct iguana_block *block;
    if ( (block= iguana_blockfind("geckohdr",virt,hash2)) != 0 )
    {
        if ( (height= block->height) >= 0 )
        {
            if ( (num= juint(valsobj,"num")) == 0 || num > virt->chain->bundlesize )
                num = virt->chain->bundlesize;
            for (i=0; i<num; i++)
            {
                if ( block != 0 )
                {
                    if ( (n= iguana_headerget(virt,&serialized[len],maxsize-len,block)) > 0 )
                        len += n;
                }
                hash2 = iguana_blockhash(virt,height+i+1);
                block = iguana_blockfind("geckohdri",virt,hash2);
            }
            return(len);
        }
    }
    return(-1);
}

void gecko_headerupdate(struct iguana_info *virt,bits256 hash2,int32_t height)
{
    int32_t bundlei; struct iguana_bundle *bp; bits256 zero;
    memset(zero.bytes,0,sizeof(zero));
    if ( (height % virt->chain->bundlesize) == 0 )
        bp = iguana_bundlecreate(virt,&bundlei,height,hash2,zero,0);
    else if ( (bp= virt->bundles[height / virt->chain->bundlesize]) != 0 )
        iguana_bundlehash2add(virt,0,bp,height % virt->chain->bundlesize,hash2);
}

char *gecko_headersarrived(struct supernet_info *myinfo,struct iguana_info *virt,char *remoteaddr,uint8_t *data,int32_t datalen,bits256 firsthash2)
{
    bits256 hash2; struct iguana_block *block; int32_t firstheight,i,len=0,n;
    n = (int32_t)(datalen / sizeof(bits256));
    if ( (block= iguana_blockfind("geckohdrs",virt,firsthash2)) != 0 && (firstheight= block->height) >= 0 )
    {
        gecko_headerupdate(virt,firsthash2,firstheight);
        for (i=0; i<n; i++)
        {
            len += iguana_rwbignum(0,&data[len],sizeof(hash2),hash2.bytes);
            gecko_headerupdate(virt,hash2,firstheight + i + 1);
        }
        return(clonestr("{\"result\":\"gecko headers processed\"}"));
    } else return(clonestr("{\"error\":\"gecko headers couldnt find firsthash2\"}"));
}

char *basilisk_respond_geckoheaders(struct supernet_info *myinfo,char *CMD,void *addr,char *remoteaddr,uint32_t basilisktag,cJSON *valsobj,uint8_t *data,int32_t datalen,bits256 hash2,int32_t from_basilisk)
{
    char *symbol; struct iguana_info *virt;
    if ( (symbol= jstr(valsobj,"symbol")) != 0 && (virt= iguana_coinfind(symbol)) != 0 )
        return(gecko_headersarrived(myinfo,virt,addr,data,datalen,hash2));
    else return(clonestr("{\"error\":\"couldt find gecko chain\"}"));
}

void gecko_requesthdrs(struct supernet_info *myinfo,struct iguana_info *virt,int32_t hdrsi)
{
    bits256 zero; struct iguana_bundle *bp=0; cJSON *vals; char *retstr;
    if ( (bp= virt->bundles[hdrsi]) != 0 )
    {
        vals = cJSON_CreateObject();
        memset(zero.bytes,0,sizeof(zero));
        jaddstr(vals,"coin",virt->symbol);
        if ( (retstr= basilisk_standardservice("HDR",myinfo,bp->hashes[0],vals,0,0)) != 0 )
            free(retstr);
        free_json(vals);
    } else printf("dont have bundle needed\n");
}

void gecko_iteration(struct supernet_info *myinfo,struct iguana_info *btcd,struct iguana_info *virt,int32_t maxmillis)
{
    char mineraddr[64]; int32_t hwmhdrsi,longesthdrsi;
    hwmhdrsi = virt->blocks.hwmchain.height / virt->chain->bundlesize;
    longesthdrsi = virt->longestchain / virt->chain->bundlesize;
    if ( hwmhdrsi < longesthdrsi )
        gecko_requesthdrs(myinfo,virt,hwmhdrsi);
    bitcoin_address(mineraddr,virt->chain->pubtype,myinfo->persistent_pubkey33,33);
    //printf("mine.%s %s\n",virt->symbol,mineraddr);
    gecko_miner(myinfo,btcd,virt,maxmillis,myinfo->persistent_pubkey33);
}
