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

// peer context, ie massively multithreaded -> bundlesQ

struct iguana_bundlereq *iguana_bundlereq(struct iguana_info *coin,struct iguana_peer *addr,int32_t type,int32_t datalen)
{
    struct iguana_bundlereq *req; int32_t allocsize;
    allocsize = (uint32_t)sizeof(*req) + datalen;
    req = mycalloc(type,1,allocsize);
    req->allocsize = allocsize;
    req->datalen = datalen;
    req->addr = addr;
    req->coin = coin;
    req->type = type;
    return(req);
}

int32_t iguana_sendblockreqPT(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_bundle *bp,int32_t bundlei,bits256 hash2,int32_t iamthreadsafe)
{
    static bits256 lastreq,lastreq2;
    int32_t len; uint8_t serialized[sizeof(struct iguana_msghdr) + sizeof(uint32_t)*32 + sizeof(bits256)];
    char hexstr[65]; init_hexbytes_noT(hexstr,hash2.bytes,sizeof(hash2));
    if ( addr == 0 || memcmp(lastreq.bytes,hash2.bytes,sizeof(hash2)) == 0 || memcmp(lastreq2.bytes,hash2.bytes,sizeof(hash2)) == 0 )
    {
        //printf("duplicate req %s or null addr.%p\n",bits256_str(hexstr,hash2),addr);
        if ( (rand() % 10 ) != 0 )
            return(0);
    }
    if ( addr->msgcounts.verack == 0 )
    {
        printf("iguana_sendblockreq (%s) hasn't verack'ed yet\n",addr->ipaddr);
        return(-1);
    }
    lastreq2 = lastreq;
    lastreq = hash2;
    if ( (len= iguana_getdata(coin,serialized,MSG_BLOCK,&hash2,1)) > 0 )
    {
        iguana_send(coin,addr,serialized,len);
        coin->numreqsent++;
        addr->pendblocks++;
        addr->pendtime = (uint32_t)time(NULL);
        if ( 0 && coin->current == bp )
            printf("REQ.%s bundlei.%d hdrsi.%d\n",bits256_str(hexstr,hash2),bundlei,bp!=0?bp->hdrsi:-1);
    } else printf("MSG_BLOCK null datalen.%d\n",len);
    return(len);
}

int32_t iguana_sendtxidreq(struct iguana_info *coin,struct iguana_peer *addr,bits256 hash2)
{
    uint8_t serialized[sizeof(struct iguana_msghdr) + sizeof(uint32_t)*32 + sizeof(bits256)];
    int32_t len,i,r,j; //char hexstr[65]; init_hexbytes_noT(hexstr,hash2.bytes,sizeof(hash2));
    if ( (len= iguana_getdata(coin,serialized,MSG_TX,&hash2,1)) > 0 )
    {
        if ( addr == 0 )
        {
            r = rand();
            for (i=0; i<coin->MAXPEERS; i++)
            {
                j = (i + r) % coin->MAXPEERS;
                addr = &coin->peers.active[j];
                if ( coin->peers.active[j].usock >= 0 && coin->peers.active[j].dead == 0 )
                {
                    iguana_send(coin,addr,serialized,len);
                    break;
                }
            }
        } else iguana_send(coin,addr,serialized,len);
    } else printf("MSG_TX null datalen.%d\n",len);
    printf("send MSG_TX.%d\n",len);
    return(len);
}

int32_t iguana_txidreq(struct iguana_info *coin,char **retstrp,bits256 txid)
{
    int32_t i;
    while ( coin->numreqtxids >= sizeof(coin->reqtxids)/sizeof(*coin->reqtxids) )
    {
        printf("txidreq full, wait\n");
        sleep(1);
    }
    char str[65]; printf("txidreq.%s\n",bits256_str(str,txid));
    coin->reqtxids[coin->numreqtxids++] = txid;
    for (i=0; i<coin->MAXPEERS; i++)
        if ( coin->peers.active[i].usock >= 0 )
            iguana_sendtxidreq(coin,coin->peers.ranked[i],txid);
    return(0);
}

void iguana_gotunconfirmedM(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_msgtx *tx,uint8_t *data,int32_t datalen)
{
    struct iguana_bundlereq *req;
    char str[65]; printf("%s unconfirmed.%s\n",addr->ipaddr,bits256_str(str,tx->txid));
    req = iguana_bundlereq(coin,addr,'U',datalen);
    req->datalen = datalen;
    req->txid = tx->txid;
    memcpy(req->serialized,data,datalen);
    queue_enqueue("recvQ",&coin->recvQ,&req->DL,0);
}

#ifdef later
struct iguana_txblock *iguana_peertxdata(struct iguana_info *coin,int32_t *bundleip,char *fname,struct OS_memspace *mem,uint32_t ipbits,bits256 hash2)
{
    int32_t bundlei,datalen,checki,hdrsi,fpos; char str[65],str2[65]; FILE *fp;
    bits256 checkhash2; struct iguana_txblock *txdata = 0; static bits256 zero;
    if ( (bundlei= iguana_peerfname(coin,&hdrsi,GLOBALTMPDIR,fname,ipbits,hash2,zero,1)) >= 0 )
    //if ( (bundlei= iguana_peerfname(coin,&hdrsi,fname,ipbits,hash2)) >= 0 )
    {
        if ( (fp= fopen(fname,"rb")) != 0 )
        {
            fseek(fp,bundlei * sizeof(bundlei),SEEK_SET);
            fread(&fpos,1,sizeof(fpos),fp);
            fseek(fp,fpos,SEEK_SET);
            fread(&checki,1,sizeof(checki),fp);
            if ( ftell(fp)-sizeof(checki) == fpos && bundlei == checki )
            {
                fread(&checkhash2,1,sizeof(checkhash2),fp);
                if ( memcmp(hash2.bytes,checkhash2.bytes,sizeof(hash2)) == 0 )
                {
                    fread(&datalen,1,sizeof(datalen),fp);
                    if ( datalen < (mem->totalsize - mem->used - 4) )
                    {
                        if ( (txdata= iguana_memalloc(mem,datalen,0)) != 0 )
                        {
                            fread(txdata,1,datalen,fp);
                            if ( txdata->datalen != datalen || txdata->block.bundlei != bundlei )
                            {
                                printf("%s peertxdata txdata->datalen.%d != %d bundlei.%d vs %d\n",bits256_str(str,txdata->block.RO.hash2),txdata->datalen,datalen,txdata->block.bundlei,bundlei);
                                getchar();
                                txdata = 0;
                                iguana_memreset(mem);
                            } //else printf("SUCCESS txdata.%s bundlei.%d fpos.%d T.%d U.%d S.%d P.%d\n",bits256_str(str,txdata->block.hash2),bundlei,fpos,txdata->numtxids,txdata->numunspents,txdata->numspends,txdata->numpkinds);
                        } else printf("peertxdata error allocating txdata\n");
                    } else printf("mismatch peertxdata datalen %d vs %ld totalsize %ld\n",datalen,mem->totalsize - mem->used - 4,(long)mem->totalsize);
                } else printf("peertxdata hash mismatch %s != %s\n",bits256_str(str,hash2),bits256_str(str2,checkhash2));
            } else printf("peertxdata bundlei.%d != checki.%d, fpos.%d ftell.%ld\n",bundlei,checki,fpos,ftell(fp));
            fclose(fp);
        } else printf("cant find file.(%s)\n",fname);
    } //else printf("bundlei.%d\n",bundlei);
    *bundleip = bundlei;
    return(txdata);
}
#endif

void iguana_gotblockM(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_txblock *origtxdata,struct iguana_msgtx *txarray,struct iguana_msghdr *H,uint8_t *data,int32_t recvlen)
{
    struct iguana_bundlereq *req; struct iguana_txblock *txdata = 0; int32_t valid,i,j,bundlei,copyflag;
    struct iguana_bundle *bp;
    if ( 0 )
    {
        for (i=0; i<txdata->space[0]; i++)
            if ( txdata->space[i] != 0 )
                break;
        if ( i != txdata->space[0] )
        {
            for (i=0; i<txdata->space[0]; i++)
                printf("%02x ",txdata->space[i]);
            printf("extra\n");
        }
    }
    if ( coin->numreqtxids > 0 )
    {
        for (i=0; i<origtxdata->block.RO.txn_count; i++)
        {
            for (j=0; j<coin->numreqtxids; j++)
            {
                if ( memcmp(coin->reqtxids[j].bytes,txarray[i].txid.bytes,sizeof(bits256)) == 0 )
                {
                    char str[65]; printf("i.%d j.%d found txid.%s\n",i,j,bits256_str(str,coin->reqtxids[j]));
                }
            }
        }
    }
    char str[65];
    if ( iguana_blockvalidate(coin,&valid,&origtxdata->block,1) < 0 )
    {
        printf("got block that doesnt validate? %s\n",bits256_str(str,origtxdata->block.RO.hash2));
        return;
    }
    else if ( 0 && coin->enableCACHE != 0 )
        printf("validated.(%s)\n",bits256_str(str,origtxdata->block.RO.hash2));
    copyflag = coin->enableCACHE;
    bp = 0, bundlei = -2;
    if ( copyflag != 0 && recvlen != 0 && ((bp= iguana_bundlefind(coin,&bp,&bundlei,origtxdata->block.RO.hash2)) == 0 || (bp->blocks[bundlei] != 0 && bp->blocks[bundlei]->fpipbits == 0)) )
    {
        req = iguana_bundlereq(coin,addr,'B',copyflag * recvlen);
        req->copyflag = 1;
        //printf("copy %p serialized[%d]\n",req,req->recvlen);
        memcpy(req->serialized,data,recvlen);
    }
    else
    {
        copyflag = 0;
        req = iguana_bundlereq(coin,addr,'B',0);
    }
    if ( bp != 0 && bundlei >= 0 && bp->blocks[bundlei] != 0 )//&& bits256_cmp(bp->blocks[bundlei]->RO.prev_block,origtxdata->block.RO.prev_block) != 0 )
    {
        if ( bp->blocks[bundlei]->fpos >= 0 && bp->blocks[bundlei]->fpipbits != 0 && bp->blocks[bundlei]->txvalid != 0 )
        {
            if ( bits256_cmp(origtxdata->block.RO.hash2,bp->blocks[bundlei]->RO.hash2) == 0 )
                return;
            else printf("mismatched tx received? mainchain.%d\n",bp->blocks[bundlei]->mainchain);
            if ( bp->blocks[bundlei]->mainchain != 0 )
                return;
        }
        bp->blocks[bundlei]->RO = origtxdata->block.RO;
        //printf("update prev for [%d:%d]\n",bp->hdrsi,bundlei);
    }
    req->recvlen = recvlen;
    req->H = *H;
    if ( bits256_cmp(origtxdata->block.RO.hash2,coin->APIblockhash) == 0 )
    {
        printf("MATCHED APIblockhash\n");
        coin->APIblockstr = calloc(1,recvlen*2+1);
        init_hexbytes_noT(coin->APIblockstr,data,recvlen);
    }
    txdata = origtxdata;
    if ( addr != 0 )
    {
        if ( addr->pendblocks > 0 )
            addr->pendblocks--;
        addr->lastblockrecv = (uint32_t)time(NULL);
        addr->recvblocks += 1.;
        addr->recvtotal += recvlen;
        if ( iguana_ramchain_data(coin,addr,origtxdata,txarray,origtxdata->block.RO.txn_count,data,recvlen) >= 0 )
        {
            txdata->block.fpipbits = (uint32_t)addr->ipbits;
            txdata->block.RO.recvlen = recvlen;
            txdata->block.fpos = 0;
            req->datalen = txdata->datalen;
            req->ipbits = txdata->block.fpipbits;
            /*if ( 0 )
            {
                struct iguana_txblock *checktxdata; struct OS_memspace checkmem; int32_t checkbundlei;
                memset(&checkmem,0,sizeof(checkmem));
                iguana_meminit(&checkmem,"checkmem",0,txdata->datalen + 4096,0);
                if ( (checktxdata= iguana_peertxdata(coin,&checkbundlei,fname,&checkmem,(uint32_t)addr->ipbits,txdata->block.RO.hash2)) != 0 )
                {
                    printf("check datalen.%d bundlei.%d T.%d U.%d S.%d P.%d X.%d\n",checktxdata->datalen,checkbundlei,checktxdata->numtxids,checktxdata->numunspents,checktxdata->numspends,checktxdata->numpkinds,checktxdata->numexternaltxids);
                }
                iguana_mempurge(&checkmem);
            }*/
        }
    }
    req->block = txdata->block;
    //printf("recvlen.%d ipbits.%x prev.(%s)\n",req->block.RO.recvlen,req->block.fpipbits,bits256_str(str,txdata->block.RO.prev_block));
    req->block.RO.txn_count = req->numtx = txdata->block.RO.txn_count;
    coin->recvcount++;
    coin->recvtime = (uint32_t)time(NULL);
    req->addr = addr;
    netBLOCKS++;
    queue_enqueue("recvQ",&coin->recvQ,&req->DL,0);
}

void iguana_gottxidsM(struct iguana_info *coin,struct iguana_peer *addr,bits256 *txids,int32_t n)
{
    struct iguana_bundlereq *req;
    //printf("got %d txids from %s\n",n,addr->ipaddr);
    req = iguana_bundlereq(coin,addr,'T',0);
    req->hashes = txids, req->n = n;
    queue_enqueue("recvQ",&coin->recvQ,&req->DL,0);
}

void iguana_gotheadersM(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_block *blocks,int32_t n)
{
    struct iguana_bundlereq *req;
    if ( addr != 0 )
    {
        addr->recvhdrs++;
        if ( addr->pendhdrs > 0 )
            addr->pendhdrs--;
        //printf("%s blocks[%d] ht.%d gotheaders pend.%d %.0f\n",addr->ipaddr,n,blocks[0].height,addr->pendhdrs,milliseconds());
    }
    req = iguana_bundlereq(coin,addr,'H',0);
    req->blocks = blocks, req->n = n;
    HDRnet++;
    //char str[65]; printf("PTblockhdrs.%s net.%d blocks.%d\n",bits256_str(str,blocks[0].RO.hash2),HDRnet,netBLOCKS);
    queue_enqueue("recvQ",&coin->recvQ,&req->DL,0);
}

void iguana_gotblockhashesM(struct iguana_info *coin,struct iguana_peer *addr,bits256 *blockhashes,int32_t n)
{
    struct iguana_bundlereq *req;
    if ( addr != 0 )
    {
        addr->recvhdrs++;
        if ( addr->pendhdrs > 0 )
            addr->pendhdrs--;
    }
    req = iguana_bundlereq(coin,addr,'S',0);
    req->hashes = blockhashes, req->n = n;
    char str[65];
    if ( 0 && n > 2 )
        printf("bundlesQ blockhashes.%s [%d]\n",bits256_str(str,blockhashes[1]),n);
    queue_enqueue("recvQ",&coin->recvQ,&req->DL,0);
}

/*void iguana_patch(struct iguana_info *coin,struct iguana_block *block)
{
    int32_t i,j,origheight,height; struct iguana_block *prev,*next; struct iguana_bundle *bp;
    prev = iguana_blockhashset(coin,-1,block->RO.prev_block,1);
    block->hh.prev = prev;
    if ( prev != 0 )
    {
        if ( prev->mainchain != 0 )
        {
            prev->hh.next = block;
            if ( memcmp(block->RO.prev_block.bytes,coin->blocks.hwmchain.RO.hash2.bytes,sizeof(bits256)) == 0 )
                _iguana_chainlink(coin,block);
            if ( (next= block->hh.next) != 0 && bits256_nonz(next->RO.hash2) > 0 )
                next->height = block->height + 1;
        }
        else if ( 0 && block->height < 0 )
        {
            for (i=0; i<1; i++)
            {
                if ( (prev= prev->hh.prev) == 0 )
                    break;
                if ( prev->mainchain != 0 && prev->height >= 0 )
                {
                    j = i;
                    origheight = (prev->height + i + 2);
                    prev = block->hh.prev;
                    height = (origheight - 1);
                    while ( i > 0 && prev != 0 )
                    {
                        if ( prev->mainchain != 0 && prev->height != height )
                        {
                            printf("mainchain height mismatch j.%d at i.%d %d != %d\n",j,i,prev->height,height);
                            break;
                        }
                        prev = prev->hh.prev;
                        height--;
                    }
                    if ( i == 0 )
                    {
                        //printf("SET HEIGHT.%d j.%d\n",origheight,j);
                        if ( (bp= coin->bundles[origheight / coin->chain->bundlesize]) != 0 )
                        {
                            iguana_bundlehash2add(coin,0,bp,origheight % coin->chain->bundlesize,block->RO.hash2);
                            block->height = origheight;
                            block->mainchain = 1;
                            prev = block->hh.prev;
                            prev->hh.next = block;
                        }
                    } //else printf("break at i.%d for j.%d origheight.%d\n",i,j,origheight);
                    break;
                }
            }
        }
    }
}*/

uint32_t iguana_allhashcmp(struct iguana_info *coin,struct iguana_bundle *bp,bits256 *blockhashes,int32_t num)
{
    bits256 allhash; int32_t err,i,n; struct iguana_block *block,*prev;
    if ( bits256_nonz(bp->allhash) > 0 && num >= coin->chain->bundlesize && bp->queued == 0 )
    {
        vcalc_sha256(0,allhash.bytes,blockhashes[0].bytes,coin->chain->bundlesize * sizeof(*blockhashes));
        if ( memcmp(allhash.bytes,bp->allhash.bytes,sizeof(allhash)) == 0 )
        {
            if ( bp->bundleheight > 0 )
                prev = iguana_blockfind(coin,iguana_blockhash(coin,bp->bundleheight-1));
            else prev = 0;
            for (i=n=0; i<coin->chain->bundlesize&&i<bp->n; i++)
            {
                if ( (err= iguana_bundlehash2add(coin,&block,bp,i,blockhashes[i])) < 0 )
                {
                    printf("error adding blockhash allhashes hdrsi.%d i.%d\n",bp->hdrsi,i);
                    return(err);
                }
                if ( block != 0 && block == bp->blocks[i] )
                {
                    if ( i > 0 )
                        block->RO.prev_block = blockhashes[i-1];
                    block->height = bp->bundleheight + i;
                    block->mainchain = 1;
                    if ( prev != 0 )
                    {
                        block->RO.prev_block = prev->RO.hash2;
                        prev->hh.next = block;
                        block->hh.prev = prev;
                    }
                    //if ( bp->hdrsi < coin->MAXBUNDLES )
                    //    iguana_blockQ(coin,bp,i,blockhashes[i],0);
                } else printf("no allhashes block.%p or mismatch.%p\n",block,bp->blocks[i]);
                prev = block;
            }
            coin->allhashes++;
           // if ( bp->hdrsi == 0 )
                printf("ALLHASHES FOUND! %d allhashes.%d\n",bp->bundleheight,coin->allhashes);
            if ( bp->queued == 0 )
                iguana_bundleQ(coin,bp,bp->n*5 + (rand() % 500));
            return(bp->queued);
        }
    }
    return(0);
}

void iguana_bundlespeculate(struct iguana_info *coin,struct iguana_bundle *bp,int32_t bundlei,bits256 hash2,int32_t offset)
{
    if ( bp == 0 )
        return;
    if ( bp->numhashes < bp->n && bundlei == 0 && bp->speculative == 0 && bp->bundleheight < coin->longestchain-coin->chain->bundlesize )
    {
        char str[65]; bits256_str(str,bp->hashes[0]);
        //fprintf(stderr,"Afound block -> %d %d hdr.%s\n",bp->bundleheight,coin->longestchain-coin->chain->bundlesize,str);
        queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(str),1);
    }
    /*else if ( bp->speculative != 0 && bundlei < bp->numspec && memcmp(hash2.bytes,bp->speculative[bundlei].bytes,sizeof(hash2)) == 0 )
    {
        bundlei += offset;
        if ( bundlei < bp->n && bundlei < bp->numspec )
        {
            //char str[65]; printf("speculative req[%d] %s\n",bundlei,bits256_str(str,bp->speculative[bundlei]));
            //iguana_blockQ(coin,0,-1,bp->speculative[bundlei],0);
        }
    } //else printf("speculative.%p %d vs %d cmp.%d\n",bp->speculative,bundlei,bp->numspec,bp->speculative!=0?memcmp(hash2.bytes,bp->speculative[bundlei].bytes,sizeof(hash2)):-1);*/
}


// main context, ie single threaded
struct iguana_bundle *iguana_bundleset(struct iguana_info *coin,struct iguana_block **blockp,int32_t *bundleip,struct iguana_block *origblock)
{
    struct iguana_block *block,*prevblock; bits256 zero,hash2,prevhash2; struct iguana_bundle *prevbp,*bp = 0; int32_t prevbundlei,bundlei = -2;
    *bundleip = -2; *blockp = 0;
    if ( origblock == 0 )
        return(0);
    memset(zero.bytes,0,sizeof(zero));
    hash2 = origblock->RO.hash2;
    if ( (block= iguana_blockhashset(coin,-1,hash2,1)) != 0 )
    {
        prevhash2 = origblock->RO.prev_block;
        if ( block != origblock )
        {
            iguana_blockcopy(coin,block,origblock);
            //fprintf(stderr,"bundleset block.%p vs origblock.%p prev.%d bits.%x fpos.%ld\n",block,origblock,bits256_nonz(prevhash2),block->fpipbits,block->fpos);
        }
        *blockp = block;
        //if ( 0 && bits256_nonz(prevhash2) > 0 )
        //    iguana_patch(coin,block);
        if ( (bp= iguana_bundlefind(coin,&bp,&bundlei,hash2)) != 0 && bundlei < coin->chain->bundlesize )
        {
            //fprintf(stderr,"bundle found %d:%d\n",bp->hdrsi,bundlei);
            block->bundlei = bundlei;
            block->hdrsi = bp->hdrsi;
            bp->blocks[bundlei] = block;
            //printf("bundlehashadd set.%d\n",bundlei);
            iguana_bundlehash2add(coin,0,bp,bundlei,hash2);
            if ( bundlei > 0 )
            {
                //printf("bundlehashadd prev %d\n",bundlei);
                iguana_bundlehash2add(coin,0,bp,bundlei-1,prevhash2);
            }
            else if ( bp->hdrsi > 0 && (bp= coin->bundles[bp->hdrsi-1]) != 0 )
                iguana_bundlehash2add(coin,0,bp,coin->chain->bundlesize-1,prevhash2);
            if ( coin->enableCACHE != 0 )
                iguana_bundlespeculate(coin,bp,bundlei,hash2,1);
        }
        prevbp = 0, prevbundlei = -2;
        iguana_bundlefind(coin,&prevbp,&prevbundlei,prevhash2);
        if ( 0 && block->blockhashes != 0 )
            fprintf(stderr,"has blockhashes bp.%p[%d] prevbp.%p[%d]\n",bp,bundlei,prevbp,prevbundlei);
        if ( prevbp != 0 && prevbundlei >= 0 && (prevblock= iguana_blockfind(coin,prevhash2)) != 0 )
        {
            if ( prevbundlei < coin->chain->bundlesize )
            {
                if ( prevbp->hdrsi+1 == coin->bundlescount && prevbundlei == coin->chain->bundlesize-1 )
                {
                    printf("AUTOCREATE.%d\n",prevbp->bundleheight + coin->chain->bundlesize);
                    bp = iguana_bundlecreate(coin,bundleip,prevbp->bundleheight + coin->chain->bundlesize,hash2,zero,0);
                    if ( bp->queued == 0 )
                        iguana_bundleQ(coin,bp,1000);
                }
                if ( prevbundlei < coin->chain->bundlesize-1 )
                {
                    //printf("bundlehash2add next %d\n",prevbundlei);
                    iguana_bundlehash2add(coin,0,prevbp,prevbundlei+1,hash2);
                }
                if ( coin->enableCACHE != 0 )
                    iguana_bundlespeculate(coin,prevbp,prevbundlei,prevhash2,2);
            }
        }
    } else printf("iguana_bundleset: error adding blockhash\n");
    bp = 0, *bundleip = -2;
    return(iguana_bundlefind(coin,&bp,bundleip,hash2));
}

struct iguana_bundlereq *iguana_recvblockhdrs(struct iguana_info *coin,struct iguana_bundlereq *req,struct iguana_block *blocks,int32_t n,int32_t *newhwmp)
{
    int32_t i,bundlei,match; bits256 *blockhashes,allhash; struct iguana_block *block; struct iguana_bundle *bp,*firstbp = 0;
    if ( blocks == 0 )
    {
        printf("iguana_recvblockhdrs null blocks?\n");
        return(req);
    }
    if ( blocks != 0 && n > 0 )
    {
        if ( 0 && n >= coin->chain->bundlesize )
        {
            blockhashes = malloc(sizeof(*blockhashes) * coin->chain->bundlesize);
            for (i=0; i<coin->chain->bundlesize; i++)
                blockhashes[i] = blocks[i].RO.hash2;
            for (i=0; i<coin->bundlescount; i++)
            {
                if ( (bp= coin->bundles[i]) != 0 && bp->emitfinish == 0 )
                {
                    blockhashes[0] = bp->hashes[0];
                    vcalc_sha256(0,allhash.bytes,blockhashes[0].bytes,coin->chain->bundlesize * sizeof(*blockhashes));
                    if ( bits256_cmp(allhash,bp->allhash) == 0 )
                    {
                        if ( bp->queued != 0 )
                            bp->queued = 0;
                        if ( iguana_allhashcmp(coin,bp,blockhashes,coin->chain->bundlesize) > 0 )
                        {
                            free(blockhashes);
                            return(req);
                        }
                    }
                }
            }
            free(blockhashes);
        }
        for (i=match=0; i<n; i++)
        {
            //fprintf(stderr,"i.%d of %d bundleset\n",i,n);
            bp = 0, bundlei = -1;
            if ( (bp= iguana_bundleset(coin,&block,&bundlei,&blocks[i])) != 0 )
            {
                //printf("{%d:%d} ",bp->hdrsi,bundlei);
                if ( i == 0 )
                    firstbp = bp;
                if ( bundlei == i+1 && bp == firstbp )
                    match++;
                else
                {
                    if ( i != n-1 )
                        fprintf(stderr,"recvhdr: ht.%d[%d] vs i.%d\n",bp->bundleheight,bundlei,i);
                }
            } else printf("blockhash[%d] cant be found\n",i);
        }
        //char str[65]; printf("blockhdrs.%s hdrsi.%d\n",bits256_str(str,blocks[0].RO.hash2),firstbp!=0?firstbp->hdrsi:-1);
        if ( firstbp != 0 && match == coin->chain->bundlesize-1 && n == firstbp->n )
        {
            if ( firstbp->queued == 0 )
            {
                //fprintf(stderr,"firstbp blockQ %d\n",firstbp->bundleheight);
                iguana_bundleQ(coin,firstbp,1000);
            }
        }
    }
    return(req);
}

struct iguana_bundlereq *iguana_recvblockhashes(struct iguana_info *coin,struct iguana_bundlereq *req,bits256 *blockhashes,int32_t num)
{
    int32_t bundlei,i,len; struct iguana_bundle *bp,*newbp; bits256 allhash,zero; char hashstr[65]; uint8_t serialized[512]; struct iguana_peer *addr; struct iguana_block *block;
    memset(zero.bytes,0,sizeof(zero));
    bp = 0, bundlei = -2;
    if ( num < 2 )
        return(req);
    iguana_bundlefind(coin,&bp,&bundlei,blockhashes[1]);
    //iguana_blockQ(coin,0,-1,blockhashes[1],0);
    //iguana_blockQ(coin,0,-4,blockhashes[1],1);
    char str[65];
    if ( num > 2 )
        printf("blockhashes[%d] %d of %d %s bp.%d[%d]\n",num,bp==0?-1:bp->hdrsi,coin->bundlescount,bits256_str(str,blockhashes[1]),bp==0?-1:bp->bundleheight,bundlei);
    if ( bp != 0 )
    {
        bp->hdrtime = (uint32_t)time(NULL);
        blockhashes[0] = bp->hashes[0];
        iguana_blockQ("recvhash0",coin,bp,0,blockhashes[0],0);
        if ( num >= coin->chain->bundlesize )
        {
            iguana_blockQ("recvhash1",coin,0,-1,blockhashes[coin->chain->bundlesize],0);
            //printf("call allhashes\n");
            if ( bp->hdrsi == coin->bundlescount-1 )
            {
                init_hexbytes_noT(hashstr,blockhashes[coin->chain->bundlesize].bytes,sizeof(bits256));
                queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(hashstr),1);
                newbp = iguana_bundlecreate(coin,&bundlei,bp->bundleheight+coin->chain->bundlesize,blockhashes[coin->chain->bundlesize],zero,1);
                if ( newbp != 0 )
                {
                    char str2[65];
                    printf("EXTEND last bundle %s/%s ht.%d\n",bits256_str(str,newbp->hashes[0]),bits256_str(str2,blockhashes[coin->chain->bundlesize]),newbp->bundleheight);
                    if ( newbp->queued == 0 )
                        iguana_bundleQ(coin,newbp,1000);
                }
            }
            else if ( iguana_allhashcmp(coin,bp,blockhashes,num) > 0 )
                return(req);
            //printf("done allhashes\n");
        }
        if ( bp != 0 && (bp->speculative == 0 || num > bp->numspec) && bp->emitfinish == 0 )
        {
            printf("FOUND speculative.%s BLOCKHASHES[%d] ht.%d\n",bits256_str(str,blockhashes[1]),num,bp->bundleheight);
            if ( bp->speculative == 0 )
                bp->speculative = mycalloc('s',bp->n+1,sizeof(*bp->speculative));
            for (i=bp->numspec; i<num&&i<=bp->n; i++)
                bp->speculative[i] = blockhashes[i];
            bp->numspec = num <= bp->n+1 ? num : bp->n+1;
            //iguana_blockQ(coin,0,-1,blockhashes[2],1);
        }
    }
    else if ( num >= coin->chain->bundlesize )
    {
        for (i=coin->bundlescount-1; i>=0; i--)
        {
            if ( (bp= coin->bundles[i]) != 0 && bp->emitfinish == 0 )
            {
                blockhashes[0] = bp->hashes[0];
                vcalc_sha256(0,allhash.bytes,blockhashes[0].bytes,coin->chain->bundlesize * sizeof(*blockhashes));
                if ( bits256_cmp(allhash,bp->allhash) == 0 )
                {
                    if ( bp->queued != 0 )
                        bp->queued = 0;
                    if ( iguana_allhashcmp(coin,bp,blockhashes,coin->chain->bundlesize) > 0 )
                    {
                        bp->hdrtime = (uint32_t)time(NULL);
                        iguana_blockQ("recvhash2",coin,bp,1,blockhashes[1],0);
                        iguana_blockQ("recvhash3",coin,bp,0,blockhashes[0],0);
                        iguana_blockQ("recvhash4",coin,bp,coin->chain->bundlesize-1,blockhashes[coin->chain->bundlesize-1],0);
                        //printf("matched bundle.%d\n",bp->bundleheight);
                        return(req);
                    } else printf("unexpected mismatch??\n");
                }
            }
        }
        //printf("no match to allhashes issue block1\n");
        struct iguana_block *block;
        if ( (block= iguana_blockhashset(coin,-1,blockhashes[1],1)) != 0 )
        {
            //block->blockhashes = blockhashes, req->hashes = 0;
            //printf("set block->blockhashes[%d]\n",num);
        }
        if ( (addr= coin->peers.ranked[0]) != 0 )
        {
            if ( (len= iguana_getdata(coin,serialized,MSG_BLOCK,&blockhashes[1],1)) > 0 )
            {
                iguana_send(coin,addr,serialized,len);
                //char str[65]; printf("REQ.%s\n",bits256_str(str,blockhashes[1]));
            }
        } else iguana_blockQ("hdr1",coin,0,-1,blockhashes[1],1);
    }
    else
    {
        if ( (block= iguana_blockfind(coin,blockhashes[1])) == 0 )
        {
            iguana_blockhashset(coin,-1,blockhashes[1],1);
            if ( (block= iguana_blockfind(coin,blockhashes[1])) != 0 )
            {
                block->newtx = 1;
                iguana_blockQ("recvhash6",coin,0,-6,blockhashes[1],1); // should be RT block
            }
        }
        else
        {
            block->newtx = 1;
            iguana_blockQ("recvhash6",coin,0,-7,blockhashes[1],0); // should be RT block
        }
    }
    return(req);
}

struct iguana_bundlereq *iguana_recvblock(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_bundlereq *req,struct iguana_block *origblock,int32_t numtx,int32_t datalen,int32_t recvlen,int32_t *newhwmp)
{
    struct iguana_bundle *bp=0; int32_t i,width,numsaved=0,bundlei = -2; struct iguana_block *block,*tmpblock,*prev; char str[65];
    if ( (bp= iguana_bundleset(coin,&block,&bundlei,origblock)) == 0 )
    {
        if ( (bp= iguana_bundlefind(coin,&bp,&bundlei,origblock->RO.hash2)) != 0 )
        {
            printf("iguana_recvblock got block [%d:%d]\n",bp->hdrsi,bundlei);
            /*if ( bits256_cmp(prev->RO.hash2,block->RO.prev_block) == 0 && bundlei < bp->n-1 )
            {
                bundlei++;
                iguana_bundlehash2add(coin,&tmpblock,bp,bundlei,block->RO.hash2);
                if ( tmpblock == block )
                {
                    printf("[%d:%d] speculative block.%p\n",bp->hdrsi,bundlei,block);
                    bp->blocks[bundlei] = block;
                    bp->hashes[bundlei] = block->RO.hash2;
                    block->bundlei = bundlei;
                    block->hdrsi = bp->hdrsi;
                    block->mainchain = prev->mainchain;
                } else printf("error adding speculative prev [%d:%d]\n",bp->hdrsi,bundlei);
            }*/
        }
        for (i=coin->bundlescount-1; i>=0; i--)
        {
            //if ( coin->bundles[i] != 0 )
            //    printf("compare vs %s\n",bits256_str(str,coin->bundles[i]->hashes[0]));
            if ( coin->bundles[i] != 0 && bits256_cmp(origblock->RO.prev_block,coin->bundles[i]->hashes[0]) == 0 )
            {
                bp = coin->bundles[i];
                bundlei = 1;
                iguana_bundlehash2add(coin,&block,bp,bundlei,origblock->RO.hash2);
                printf("iguana_recvblock [%d] bundlehashadd set.%d block.%p\n",i,bundlei,block);
                if ( block != 0 )
                {
                    bp->blocks[bundlei] = block;
                    block->bundlei = bundlei;
                    block->hdrsi = bp->hdrsi;
                }
                break;
            }
        }
        //printf("i.%d ref prev.(%s)\n",i,bits256_str(str,origblock->RO.prev_block));
    }
    else if ( bp == coin->current && bp != 0 && block != 0 && bundlei >= 0 )
    {
        if ( bp->speculative != 0 && bp->numspec <= bundlei )
        {
            bp->speculative[bundlei] = block->RO.hash2;
            bp->numspec = bundlei+1;
        }
        if ( block != 0 && bundlei > 0 && (prev= iguana_blockfind(coin,block->RO.prev_block)) != 0 )
        {
            if ( bp->bundleheight+bundlei-1 >= coin->blocks.hwmchain.height )
            {
                printf("prev issue.%s\n",bits256_str(str,prev->RO.hash2));
                iguana_blockQ("previssue",coin,bp,bundlei-1,prev->RO.hash2,0);
            }
        }
    }
    if ( 0 )//&& bp != 0 && bp->hdrsi == coin->bundlescount-1 )
    {
        int32_t i; static int32_t numrecv;
        numrecv++;
        if ( bp != 0 )
        {
            for (i=numsaved=0; i<bp->n; i++)
                if ( (tmpblock= bp->blocks[i]) != 0 && tmpblock->fpipbits != 0 && tmpblock->fpos >= 0 && ((bp->hdrsi == 0 && i == 0) || bits256_nonz(tmpblock->RO.prev_block) != 0) )
                    numsaved++;
        }
        fprintf(stderr,"%s [%d:%d] block.%x | s.%d r.%d copy.%d\n",bits256_str(str,origblock->RO.hash2),bp!=0?bp->hdrsi:-1,bundlei,block->fpipbits,numsaved,numrecv,req->copyflag);
    }
    if ( 1 && bundlei == 1 && bp != 0 && bp->numhashes < bp->n && coin->enableCACHE != 0 )
    {
        //printf("reissue hdrs request for [%d]\n",bp->hdrsi);
        queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(bits256_str(str,bp->hashes[0])),1);
    }
    if ( (block= iguana_blockhashset(coin,-1,origblock->RO.hash2,1)) != 0 )
    {
        if ( block != origblock )
            iguana_blockcopy(coin,block,origblock);
        if ( block->newtx != 0 )
        {
            if ( (prev= iguana_blockfind(coin,block->RO.prev_block)) == 0 )
                prev = iguana_blockhashset(coin,-1,block->RO.prev_block,1);
            width = coin->chain->bundlesize;
            while ( prev != 0 && width-- > 0 )
            {
                if ( prev->mainchain != 0 )
                    break;
                if ( prev->fpipbits == 0 )
                {
                    //printf("width.%d auto prev newtx %s\n",width,bits256_str(str,prev->RO.hash2));
                    prev->newtx = 1;
                    iguana_blockQ("autoprev",coin,0,-1,prev->RO.hash2,0);
                }
                if ( bits256_nonz(prev->RO.prev_block) != 0 )
                {
                    if ( (prev= iguana_blockfind(coin,prev->RO.prev_block)) == 0 )
                        prev = iguana_blockhashset(coin,-1,prev->RO.prev_block,1);
                    prev->newtx = 1;
                } else prev = 0;
            }
        }
        if ( req->copyflag != 0 )
        {
            if ( block->queued == 0 && bp != 0 )
            {
                //char str[65]; fprintf(stderr,"req.%p %s copyflag.%d %d data %d %d\n",req,bits256_str(str,block->RO.hash2),req->copyflag,block->height,req->recvlen,recvlen);
                coin->numcached++;
                block->queued = 1;
                queue_enqueue("cacheQ",&coin->cacheQ,&req->DL,0);
                return(0);
            }
            else if ( block->req == 0 )
            {
                block->req = req;
                req = 0;
            } //else printf("already have cache entry.(%s)\n",bits256_str(str,origblock->RO.hash2));
        }
        //printf("datalen.%d ipbits.%x\n",datalen,req->ipbits);
    } else printf("cant create origblock.%p block.%p bp.%p bundlei.%d\n",origblock,block,bp,bundlei);
    return(req);
}

struct iguana_bundlereq *iguana_recvtxids(struct iguana_info *coin,struct iguana_bundlereq *req,bits256 *txids,int32_t n)
{
    char str[65];
    if ( n > 0 )
        printf("got txids[%d] %s\n",n,bits256_str(str,txids[0]));
    return(req);
}

struct iguana_bundlereq *iguana_recvunconfirmed(struct iguana_info *coin,struct iguana_bundlereq *req,uint8_t *data,int32_t datalen)
{
    int32_t i;
    for (i=0; i<coin->numreqtxids; i++)
    {
        if ( memcmp(req->txid.bytes,coin->reqtxids[i].bytes,sizeof(req->txid)) == 0 )
        {
            char str[65]; printf("got reqtxid.%s datalen.%d | numreqs.%d\n",bits256_str(str,req->txid),req->datalen,coin->numreqtxids);
            coin->reqtxids[i] = coin->reqtxids[--coin->numreqtxids];
        }
    }
    return(req);
}

int32_t iguana_blockreq(struct iguana_info *coin,int32_t height,int32_t priority)
{
    int32_t hdrsi,bundlei; struct iguana_bundle *bp;
    hdrsi = height / coin->chain->bundlesize;
    bundlei = height % coin->chain->bundlesize;
    if ( (bp= coin->bundles[hdrsi]) != 0 && bits256_nonz(bp->hashes[bundlei]) != 0 )
    {
        iguana_blockQ("blockreq",coin,bp,bundlei,bp->hashes[bundlei],priority);
        return(height);
    }
    return(-1);
}

int32_t iguana_reqblocks(struct iguana_info *coin)
{
    int32_t hdrsi,lflag,bundlei,flag = 0; bits256 hash2; struct iguana_block *next,*block; struct iguana_bundle *bp;
    /*if ( 0 && (bp= coin->current) != 0 && bp->numsaved < bp->n )
    {
        for (hdrsi=numissued=0; hdrsi<coin->MAXBUNDLES && coin->current->hdrsi+hdrsi<coin->bundlescount && numissued<100; hdrsi++)
        {
            if ( (bp= coin->bundles[hdrsi + coin->current->hdrsi]) == 0 )
                continue;
            if ( (addr= coin->peers.ranked[hdrsi]) == 0 || addr->msgcounts.verack == 0 )
                continue;
            for (bundlei=n=flag=0; bundlei<bp->n; bundlei++)
                if ( (block= bp->blocks[bundlei]) != 0 )
                {
                    if ( bits256_nonz(block->RO.hash2) > 0 && block->fpos >= 0 )
                        n++;
                    else if ( block->fpipbits == 0 || time(NULL) > block->issued+60 )
                    {
                        block->issued = (uint32_t)time(NULL);
                        //iguana_sendblockreqPT(coin,addr,bp,bundlei,block->RO.hash2,0);
                        iguana_blockQ("reqblocks",coin,bp,bundlei,block->RO.hash2,0);
                        flag++;
                        if ( ++numissued > 100 )
                            break;
                    }
                }
            if ( 0 && flag != 0 )
                printf("issued %d priority blocks for %d current.[%d] have %d blocks emit.%u\n",flag,hdrsi,bp->hdrsi,n,bp->emitfinish);
        }
    }*/
    hdrsi = (coin->blocks.hwmchain.height+1) / coin->chain->bundlesize;
    if ( (bp= coin->bundles[hdrsi]) != 0 )
    {
        bundlei = (coin->blocks.hwmchain.height+1) % coin->chain->bundlesize;
        if ( (next= bp->blocks[bundlei]) != 0 || (next= iguana_blockfind(coin,bp->hashes[bundlei])) != 0 )
        {
            if ( bits256_nonz(next->RO.prev_block) > 0 )
                _iguana_chainlink(coin,next);
            else if ( next->queued == 0 && next->fpipbits == 0 && (rand() % 100) == 0 )
            {
                //printf("HWM next %d\n",coin->blocks.hwmchain.height+1);
                iguana_blockQ("reqblocks",coin,bp,bundlei,next->RO.hash2,0);
            }
        }
        /*else if ( iguana_blockfind(coin,bp->hashes[bundlei]) == 0 )
        {
            //if ( bits256_nonz(bp->hashes[bundlei]) > 0 )
            // {
            // printf("next %d\n",coin->blocks.hwmchain.height+1);
            // iguana_blockQ(coin,bp,bundlei,bp->hashes[bundlei],0);
            // }
            // else if ( bp->speculative != 0 && (bits256_cmp(bp->hashes[bundlei],bp->speculative[bundlei]) != 0 || (rand() % 100) == 0) )
             {
                 if ( time(NULL) > bp->issued[bundlei]+30 && iguana_blockfind(coin,bp->speculative[bundlei]) == 0 )
                 {
                     bp->hashes[bundlei] = bp->speculative[bundlei];
                     struct iguana_bloominds bit = iguana_calcbloom(bp->speculative[bundlei]);
                     if ( iguana_bloomfind(coin,&bp->bloom,0,bit) < 0 )
                         iguana_bloomset(coin,&bp->bloom,0,bit);
                     printf("speculative next %d\n",coin->blocks.hwmchain.height+1);
                     iguana_blockQ("speculativenext",coin,0,-1,bp->speculative[bundlei],0);
                     bp->issued[bundlei] = (uint32_t)time(NULL);
                 }
             }
        }*/
    }
    /*else if ( 0 && (bp= coin->bundles[--hdrsi]) != 0 )
    {
        char str[65];
        queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(bits256_str(str,bp->hashes[0])),1);
    }*/
    lflag = 1;
    while ( lflag != 0 )
    {
        lflag = 0;
        hdrsi = (coin->blocks.hwmchain.height+1) / coin->chain->bundlesize;
        bundlei = (coin->blocks.hwmchain.height+1) % coin->chain->bundlesize;
        if ( (next= iguana_blockfind(coin,iguana_blockhash(coin,coin->blocks.hwmchain.height+1))) == 0 )
        {
            if ( (block= iguana_blockfind(coin,coin->blocks.hwmchain.RO.hash2)) != 0 )
                next = block->hh.next; //, next/block->mainchain = 1;
        }
        if ( next == 0 && hdrsi < coin->bundlescount && (bp= coin->bundles[hdrsi]) != 0 && (next= bp->blocks[bundlei]) != 0 )
        {
            if ( bits256_nonz(next->RO.prev_block) == 0 )
            {
                printf(" next has null prev [%d:%d]\n",bp->hdrsi,bundlei);
                iguana_blockQ("reqblocks0",coin,bp,bundlei,next->RO.hash2,0);
                next = 0;
            }
        }
        /*else if ( bp != 0 && bits256_nonz(bp->hashes[bundlei]) == 0 && time(NULL) > bp->issued[bundlei]+60 )
        {
            if ( bundlei > 0 && bits256_nonz(bp->hashes[bundlei+1]) != 0 )
            {
                if ( (block= iguana_blockfind(coin,bp->hashes[bundlei+1])) != 0 && bits256_nonz(block->RO.prev_block) != 0 )
                {
                    bp->hashes[bundlei] = block->RO.prev_block;
                    printf("reqblock [%d:%d]\n",bp->hdrsi,bundlei);
                    iguana_blockQ("reqblocks1",coin,bp,bundlei,bp->hashes[bundlei],0);
                }
            }
        }*/
        if ( next != 0 )
        {
            //printf("have next %d\n",coin->blocks.hwmchain.height);
            if ( memcmp(next->RO.prev_block.bytes,coin->blocks.hwmchain.RO.hash2.bytes,sizeof(bits256)) == 0 )
            {
                if ( _iguana_chainlink(coin,next) != 0 )
                    lflag++, flag++;
                //else printf("chainlink error for %d\n",coin->blocks.hwmchain.height+1);
            }
        }
        if ( 1 )//queue_size(&coin->blocksQ) < _IGUANA_MAXPENDING )
        {
            /*double threshold,lag = OS_milliseconds() - coin->backstopmillis;
             threshold = (10 + coin->longestchain - coin->blocksrecv);
             if ( threshold < 1 )
             threshold = 1.;
             if ( (bp= coin->bundles[(coin->blocks.hwmchain.height+1)/coin->chain->bundlesize]) != 0 )
             threshold = (bp->avetime + coin->avetime) * .5;
             else threshold = coin->avetime;
             threshold *= 100. * sqrt(threshold) * .000777;*/
            double threshold,lag = OS_milliseconds() - coin->backstopmillis;
            threshold = 300;
            if ( coin->blocks.hwmchain.height < coin->longestchain && (coin->backstop != coin->blocks.hwmchain.height+1 || lag > threshold) )
            {
                coin->backstop = coin->blocks.hwmchain.height+1;
                hash2 = iguana_blockhash(coin,coin->backstop);
                bp = coin->bundles[(coin->blocks.hwmchain.height+1)/coin->chain->bundlesize];
                bundlei = (coin->blocks.hwmchain.height+1) % coin->chain->bundlesize;
                if ( bp != 0 && bits256_nonz(hash2) == 0 )
                {
                    hash2 = bp->hashes[bundlei];
                    if ( bits256_nonz(hash2) == 0 && bp->speculative != 0 )
                    {
                        hash2 = bp->speculative[bundlei];
                        if ( bits256_nonz(hash2) > 0 )
                        {
                            if ( (block= iguana_blockfind(coin,hash2)) != 0 && bits256_cmp(block->RO.prev_block,coin->blocks.hwmchain.RO.hash2) == 0 )
                            {
                                printf("speculative is next at %d\n",coin->backstop);
                                if ( _iguana_chainlink(coin,block) != 0 )
                                    lflag++, flag++, printf("NEWHWM.%d\n",coin->backstop);
                            }
                        }
                    }
                }
                if ( bits256_nonz(hash2) > 0 )
                {
                    if ( bp != 0 && bits256_nonz(hash2) > 0 )
                    {
                        coin->backstopmillis = OS_milliseconds();
                        iguana_blockQ("mainchain",coin,0,-1,hash2,lag > 100 * threshold);
                        flag++;
                        char str[65];
                        if ( 1 && (rand() % 1000) == 0 || bp->bundleheight > coin->longestchain-coin->chain->bundlesize )
                            printf("%s MAINCHAIN.%d threshold %.3f %.3f lag %.3f\n",bits256_str(str,hash2),coin->blocks.hwmchain.height+1,threshold,coin->backstopmillis,lag);
                    }
                }
                else if ( bp != 0 && bundlei < bp->n-1 && (bits256_nonz(bp->hashes[bundlei+1]) != 0 || (bp->speculative != 0 && bits256_nonz(bp->speculative[bundlei+1]) != 0)) )
                {
                    if ( time(NULL) > bp->issued[bundlei+1]+10 )
                    {
                        bp->issued[bundlei+1] = (uint32_t)time(NULL);
                        printf("MAINCHAIN skip issue %d\n",bundlei+1);
                        if ( bits256_nonz(bp->hashes[bundlei+1]) != 0 )
                            iguana_blockQ("mainskip",coin,bp,bundlei,bp->hashes[bundlei+1],0);
                        else if ( bp->speculative != 0 && bundlei+1 < bp->numspec )
                            iguana_blockQ("mainskip",coin,bp,bundlei,bp->speculative[bundlei+1],0);
                    }
                }
                else if ( bp != 0 && time(NULL) > bp->hdrtime+10 )
                {
                    char str[65];
                    //printf("MAINCHAIN gethdr %d %s\n",bp->bundleheight,bits256_str(str,bp->hashes[0]));
                    queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(bits256_str(str,bp->hashes[0])),1);
                    bp->hdrtime = (uint32_t)time(NULL);
                }
            }
        }
    }
    return(flag);
}

int32_t iguana_processrecvQ(struct iguana_info *coin,int32_t *newhwmp) // single threaded
{
    int32_t flag = 0; struct iguana_bundlereq *req;
    *newhwmp = 0;
    while ( (req= queue_dequeue(&coin->recvQ,0)) != 0 )
    {
        //fprintf(stderr,"%s recvQ.%p type.%c n.%d\n",req->addr != 0 ? req->addr->ipaddr : "0",req,req->type,req->n);
        if ( req->type == 'B' ) // one block with all txdata
        {
            netBLOCKS--;
            req = iguana_recvblock(coin,req->addr,req,&req->block,req->numtx,req->datalen,req->recvlen,newhwmp);
            flag++;
        }
        else if ( req->type == 'H' ) // blockhdrs (doesnt have txn_count!)
        {
            HDRnet--;
            if ( (req= iguana_recvblockhdrs(coin,req,req->blocks,req->n,newhwmp)) != 0 )
            {
                if ( req->blocks != 0 )
                    myfree(req->blocks,sizeof(*req->blocks) * req->n), req->blocks = 0;
            }
        }
        else if ( req->type == 'S' ) // blockhashes
        {
            if ( (req= iguana_recvblockhashes(coin,req,req->hashes,req->n)) != 0 && req->hashes != 0 )
                myfree(req->hashes,sizeof(*req->hashes) * req->n), req->hashes = 0;
        }
        else if ( req->type == 'U' ) // unconfirmed tx
            req = iguana_recvunconfirmed(coin,req,req->serialized,req->datalen);
        else if ( req->type == 'T' ) // txids from inv
        {
            if ( (req= iguana_recvtxids(coin,req,req->hashes,req->n)) != 0 )
                myfree(req->hashes,(req->n+1) * sizeof(*req->hashes)), req->hashes = 0;
        }
        else printf("iguana_updatebundles unknown type.%c\n",req->type), getchar();
        //fprintf(stderr,"finished coin->recvQ\n");
        if ( req != 0 )
            myfree(req,req->allocsize), req = 0;
        if ( flag >= IGUANA_BUNDLELOOP )
            break;
        if ( (flag % 100) == 0 )
            iguana_reqblocks(coin);
    }
    iguana_reqblocks(coin);
    return(flag);
}

int32_t iguana_needhdrs(struct iguana_info *coin)
{
    if ( coin->longestchain == 0 || coin->blocks.hashblocks < coin->longestchain-coin->chain->bundlesize )
        return(1);
    else return(0);
}

int32_t iguana_reqhdrs(struct iguana_info *coin)
{
    int32_t i,lag,n = 0; struct iguana_bundle *bp; char hashstr[65];
    if ( queue_size(&coin->hdrsQ) == 0 )
    {
        //if ( iguana_needhdrs(coin) > 0 )
        {
            for (i=0; i<coin->bundlescount; i++)
            {
                if ( (bp= coin->bundles[i]) != 0 && (bp == coin->current || i == coin->bundlescount-1 || bp->numhashes < bp->n) )
                {
                    lag = 30;
                    if ( bp->bundleheight < coin->longestchain && time(NULL) > bp->issuetime+lag )
                    {
                        //printf("LAG.%ld hdrsi.%d numhashes.%d:%d needhdrs.%d qsize.%d zcount.%d\n",time(NULL)-bp->hdrtime,i,bp->numhashes,bp->n,iguana_needhdrs(coin),queue_size(&coin->hdrsQ),coin->zcount);
                        if ( bp->issuetime == 0 )
                            coin->numpendings++;
                        init_hexbytes_noT(hashstr,bp->hashes[0].bytes,sizeof(bits256));
                        queue_enqueue("hdrsQ",&coin->hdrsQ,queueitem(hashstr),1);
                        printf("hdrsi.%d reqHDR.(%s) numhashes.%d\n",bp->hdrsi,hashstr,bp->numhashes);
                        if ( 1 )
                        {
                            iguana_blockQ("reqhdrs0",coin,bp,0,bp->hashes[0],0);
                            if ( bits256_nonz(bp->hashes[1]) > 0 )
                                iguana_blockQ("reqhdrs1",coin,bp,1,bp->hashes[1],0);
                        }
                        n++;
                        bp->hdrtime = bp->issuetime = (uint32_t)time(NULL);
                    }
                }
            }
            if ( 0 && n > 0 )
                printf("REQ HDRS pending.%d\n",n);
            coin->zcount = 0;
        }
    } else coin->zcount = 0;
    return(n);
}

struct iguana_blockreq { struct queueitem DL; bits256 hash2,*blockhashes; struct iguana_bundle *bp; int32_t n,height,bundlei; };

int32_t iguana_blockQ(char *argstr,struct iguana_info *coin,struct iguana_bundle *bp,int32_t bundlei,bits256 hash2,int32_t priority)
{
    queue_t *Q; char *str; int32_t height = -1; struct iguana_blockreq *req; struct iguana_block *block = 0;
    if ( bits256_nonz(hash2) == 0 )
    {
        printf("cant queue zerohash bundlei.%d\n",bundlei);
        //getchar();
        return(-1);
    }
    block = iguana_blockfind(coin,hash2);
    if ( priority != 0 || block == 0 || (block->queued == 0 && block->fpipbits == 0) )
    {
        if ( bp != 0 && bundlei >= 0 && bundlei < bp->n )
        {
            if ( block == 0 )
                block = bp->blocks[bundlei];
            height = bp->bundleheight + bundlei;
        }
        if ( block != 0 )
        {
            if ( bits256_cmp(coin->APIblockhash,hash2) != 0 && (block->fpipbits != 0 || block->req != 0 || block->queued != 0) )
            {
                if ( block->fpipbits == 0 && block->queued == 0 && block->req != 0 )
                {
                    block->queued = 1;
                    queue_enqueue("cacheQ",&coin->cacheQ,&block->req->DL,0);
                    block->req = 0;
                    //char str2[65]; printf("already have.(%s)\n",bits256_str(str2,block->RO.hash2));
                }
                return(0);
            }
            height = block->height;
        }
        if ( bp != 0 && bp->emitfinish != 0 )
            return(0);
        if ( priority != 0 )
            str = "priorityQ", Q = &coin->priorityQ;
        else str = "blocksQ", Q = &coin->blocksQ;
        if ( Q != 0 )
        {
            req = mycalloc('y',1,sizeof(*req));
            req->hash2 = hash2;
            if ( (req->bp= bp) != 0 && bundlei >= 0 )
            {
                height = bp->bundleheight + bundlei;
                bp->issued[bundlei] = (uint32_t)time(NULL);
            }
            req->height = height;
            req->bundlei = bundlei;
            char str2[65];
            if ( 0 && (bundlei % 250) == 0 )
            printf("%s %s [%d:%d] %d %s %d numranked.%d qsize.%d\n",argstr,str,bp!=0?bp->hdrsi:-1,bundlei,req->height,bits256_str(str2,hash2),coin->blocks.recvblocks,coin->peers.numranked,queue_size(Q));
            if ( block != 0 )
            {
                block->numrequests++;
                block->issued = (uint32_t)time(NULL);
            }
            queue_enqueue(str,Q,&req->DL,0);
            return(1);
        } else printf("null Q\n");
    } //else printf("queueblock skip priority.%d bundlei.%d\n",bundlei,priority);
    return(0);
}

int32_t iguana_pollQsPT(struct iguana_info *coin,struct iguana_peer *addr)
{
    uint8_t serialized[sizeof(struct iguana_msghdr) + sizeof(uint32_t)*32 + sizeof(bits256)];
    struct iguana_block *block; struct iguana_blockreq *req=0; char *hashstr=0; bits256 hash2;
    int32_t bundlei,priority,i,m,z,pend,limit,height=-1,datalen,flag = 0;
    uint32_t now; struct iguana_bundle *bp; struct iguana_peer *ptr;
    if ( addr->msgcounts.verack == 0 )
        return(0);
    now = (uint32_t)time(NULL);
    if ( iguana_needhdrs(coin) != 0 && addr->pendhdrs < IGUANA_MAXPENDHDRS )
    {
        //printf("%s check hdrsQ\n",addr->ipaddr);
        if ( (hashstr= queue_dequeue(&coin->hdrsQ,1)) != 0 )
        {
            if ( (datalen= iguana_gethdrs(coin,serialized,coin->chain->gethdrsmsg,hashstr)) > 0 )
            {
                decode_hex(hash2.bytes,sizeof(hash2),hashstr);
                if ( bits256_nonz(hash2) > 0 )
                {
                    bp = 0, bundlei = -2;
                    bp = iguana_bundlefind(coin,&bp,&bundlei,hash2);
                    z = m = 0;
                    if ( bp != 0 )
                    {
                        if ( bp->bundleheight+coin->chain->bundlesize < coin->longestchain )
                        {
                            m = (coin->longestchain - bp->bundleheight);
                            if ( bp->numhashes < m )
                                z = 1;
                        }
                        else if ( bp->numhashes < bp->n )
                            z = 1;
                    }
                    if ( bp == 0 || z != 0 || bp == coin->current )
                    {
                        //printf("%s request HDR.(%s) numhashes.%d\n",addr!=0?addr->ipaddr:"local",hashstr,bp!=0?bp->numhashes:0);
                        iguana_send(coin,addr,serialized,datalen);
                        addr->pendhdrs++;
                        flag++;
                    } else printf("skip hdrreq.%s m.%d z.%d bp.%p longest.%d queued.%d\n",hashstr,m,z,bp,bp->coin->longestchain,bp->queued);
                }
                //free_queueitem(hashstr);
                //return(flag);
            } else printf("datalen.%d from gethdrs\n",datalen);
            free_queueitem(hashstr);
            hashstr = 0;
        }
    }
    //if ( netBLOCKS > coin->MAXPEERS*coin->MAXPENDING )
    //    usleep(netBLOCKS);
    if ( (limit= addr->recvblocks) > coin->MAXPENDING )
        limit = coin->MAXPENDING;
    if ( limit < 1 )
        limit = 1;
    if ( addr->pendblocks >= limit )
    {
        //printf("%s %d overlimit.%d\n",addr->ipaddr,addr->pendblocks,limit);
        return(0);
    }
    priority = 1;
    req = queue_dequeue(&coin->priorityQ,0);
    if ( flag == 0 && req == 0 && addr->pendblocks < limit )
    {
        priority = 0;
        for (i=m=pend=0; i<coin->peers.numranked; i++)
        {
            if ( (ptr= coin->peers.ranked[i]) != 0 && ptr->msgcounts.verack > 0 )
                pend += ptr->pendblocks, m++;
        }
        if ( pend < coin->MAXPENDING*m )
            req = queue_dequeue(&coin->blocksQ,0);
    }
    if ( req != 0 )
    {
        hash2 = req->hash2;
        height = req->height;
        if ( (bp= req->bp) != 0 && req->bundlei >= 0 && req->bundlei < bp->n )
            block = bp->blocks[req->bundlei];
        else block = 0;
        if ( priority == 0 && bp != 0 && req->bundlei >= 0 && req->bundlei < bp->n && req->bundlei < coin->chain->bundlesize && block != 0 && (block->fpipbits != 0 || block->queued != 0) )
        {
            if ( 1 && priority != 0 )
                printf("SKIP %p[%d] %d\n",bp,bp!=0?bp->bundleheight:-1,req->bundlei);
        }
        else
        {
            if ( block != 0 )
                block->numrequests++;
            iguana_sendblockreqPT(coin,addr,bp,req->bundlei,hash2,0);
        }
        flag++;
        myfree(req,sizeof(*req));
    }
    return(flag);
}

int32_t iguana_processrecv(struct iguana_info *coin) // single threaded
{
    int32_t newhwm = 0,flag = 0;
    //fprintf(stderr,"process coin->recvQ\n");
    flag += iguana_processrecvQ(coin,&newhwm);
    //fprintf(stderr,"iguana_reqhdrs\n");
    flag += iguana_reqhdrs(coin);
    //fprintf(stderr,"iguana_reqblocks\n");
    //flag += iguana_reqblocks(coin);
    return(flag);
}
