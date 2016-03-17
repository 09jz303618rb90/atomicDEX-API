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
const char *Hardcoded_coins[][3] = { { "BTC", "bitcoin", "0" }, { "BTCD", "BitcoinDark", "129" },  { "VPN", "VPNcoin", "129" }, { "LTC", "litecoin", "129" } , { "endmarker", "", "" } };

struct iguana_info *iguana_coinfind(const char *symbol)
{
    int32_t i;
    for (i=0; i<sizeof(Coins)/sizeof(*Coins); i++)
    {
        if ( Coins[i] != 0 && strcmp(Coins[i]->symbol,symbol) == 0 )
            return(Coins[i]);
    }
    return(0);
}

struct iguana_info *iguana_coinadd(const char *symbol,cJSON *argjson)
{
    struct iguana_info *coin; int32_t i = 0;
    if ( symbol == 0 )
    {
        for (i=0; i<sizeof(Coins)/sizeof(*Coins); i++)
            if ( Hardcoded_coins[i][0] == 0 )
                break;
        for (; i<sizeof(Coins)/sizeof(*Coins); i++)
        {
            if ( Coins[i] == 0 )
            {
                Coins[i] = mycalloc('c',1,sizeof(*Coins[i]));
                printf("iguana_coin.(new) -> %p\n",Coins[i]);
                return(Coins[i]);
            } return(0);
            printf("i.%d (%s) vs name.(%s)\n",i,Coins[i]->name,symbol);
        }
    }
    else
    {
        for (i=0; i<sizeof(Coins)/sizeof(*Coins); i++)
        {
            if ( i >= sizeof(Hardcoded_coins)/sizeof(*Hardcoded_coins) )
                break;
            //printf("Hardcoded_coins[i][0] %s vs.(%s)\n",Hardcoded_coins[i][0],symbol);
            //if ( symbol[0] == 0 )
            //    getchar();
            if ( strcmp("endmarker",Hardcoded_coins[i][0]) == 0 || strcmp(symbol,Hardcoded_coins[i][0]) == 0 )
            {
                if ( Coins[i] == 0 )
                    Coins[i] = mycalloc('c',1,sizeof(*Coins[i]));
                coin = Coins[i];
                if ( coin->chain == 0 )
                {
                    if ( i < sizeof(Hardcoded_coins)/sizeof(*Hardcoded_coins) )
                        strcpy(coin->name,Hardcoded_coins[i][1]);
                    else if (argjson != 0 )
                    {
                        if ( jstr(argjson,"name") != 0 )
                            safecopy(coin->name,jstr(argjson,"name"),sizeof(coin->name));
                        else strcpy(coin->name,symbol);
                    }
                    coin->chain = iguana_chainfind((char *)symbol,argjson,1);
                    strcpy(coin->symbol,symbol);
                    iguana_initcoin(coin,argjson);
                }
                return(coin);
            }
        }
    }
    return(0);
}

struct iguana_info *iguana_coinselect()
{
    int32_t i;
    for (i=0; i<sizeof(Coins)/sizeof(*Coins); i++)
    {
        if ( Coins[i] != 0 && Coins[i]->symbol[0] != 0 && Coins[i]->bundlescount > 0 )
            return(Coins[i]);
    }
    return(0);
}

void iguana_recvalloc(struct iguana_info *coin,int32_t numitems)
{
    //coin->blocks.ptrs = myrealloc('W',coin->blocks.ptrs,coin->blocks.ptrs==0?0:coin->blocks.maxbits * sizeof(*coin->blocks.ptrs),numitems * sizeof(*coin->blocks.ptrs));
    coin->blocks.RO = myrealloc('W',coin->blocks.RO,coin->blocks.RO==0?0:coin->blocks.maxbits * sizeof(*coin->blocks.RO),numitems * sizeof(*coin->blocks.RO));
    printf("realloc waitingbits.%d -> %d\n",coin->blocks.maxbits,numitems);
    coin->blocks.maxbits = numitems;
}

static int _decreasing_double(const void *a,const void *b)
{
#define double_a (*(double *)a)
#define double_b (*(double *)b)
	if ( double_b > double_a )
		return(1);
	else if ( double_b < double_a )
		return(-1);
	return(0);
#undef double_a
#undef double_b
}

static int32_t revsortds(double *buf,uint32_t num,int32_t size)
{
	qsort(buf,num,size,_decreasing_double);
	return(0);
}

double iguana_metric(struct iguana_peer *addr,uint32_t now,double decay)
{
    int32_t duration; double metric = addr->recvblocks * addr->recvtotal;
    addr->recvblocks *= decay;
    addr->recvtotal *= decay;
    if ( now >= addr->ready && addr->ready != 0 )
        duration = (now - addr->ready + 1);
    else duration = 1;
    if ( metric < SMALLVAL && duration > 300 )
        metric = 0.001;
    else metric /= duration;
    return(metric);
}

int32_t iguana_peermetrics(struct iguana_info *coin)
{
    int32_t i,ind,n; double *sortbuf,sum; uint32_t now; struct iguana_peer *addr,*slowest = 0;
    //printf("peermetrics\n");
    sortbuf = mycalloc('s',coin->MAXPEERS,sizeof(double)*2);
    coin->peers.mostreceived = 0;
    now = (uint32_t)time(NULL);
    for (i=n=0; i<coin->MAXPEERS; i++)
    {
        addr = &coin->peers.active[i];
        if ( addr->usock < 0 || addr->dead != 0 || addr->ready == 0 )
            continue;
        addr->pendblocks = 0;
        if ( addr->recvblocks > coin->peers.mostreceived )
            coin->peers.mostreceived = addr->recvblocks;
        //printf("[%.0f %.0f] ",addr->recvblocks,addr->recvtotal);
        sortbuf[n*2 + 0] = iguana_metric(addr,now,1.);
        sortbuf[n*2 + 1] = i;
        n++;
    }
    if ( n > 0 )
    {
        revsortds(sortbuf,n,sizeof(double)*2);
        portable_mutex_lock(&coin->peers_mutex);
        for (sum=i=0; i<n; i++)
        {
            if ( i < coin->MAXPEERS )
            {
                coin->peers.topmetrics[i] = sortbuf[i*2];
                ind = (int32_t)sortbuf[i*2 +1];
                coin->peers.ranked[i] = &coin->peers.active[ind];
                if ( sortbuf[i*2] > SMALLVAL && (double)i/n > .8 )
                    slowest = coin->peers.ranked[i];
                //printf("(%.5f %s) ",sortbuf[i*2],coin->peers.ranked[i]->ipaddr);
                coin->peers.ranked[i]->rank = i + 1;
                sum += coin->peers.topmetrics[i];
            }
        }
        coin->peers.numranked = n;
        portable_mutex_unlock(&coin->peers_mutex);
        //printf("NUMRANKED.%d\n",n);
        if ( i > 0 )
        {
            coin->peers.avemetric = (sum / i);
            if ( i >= (coin->MAXPEERS - 1) && slowest != 0 )
            {
                printf("prune slowest peer.(%s) numranked.%d\n",slowest->ipaddr,n);
                slowest->dead = 1;
            }
        }
    }
    myfree(sortbuf,coin->MAXPEERS * sizeof(double) * 2);
    return(coin->peers.mostreceived);
}

void *iguana_kviAddriterator(struct iguana_info *coin,struct iguanakv *kv,struct iguana_kvitem *item,uint64_t args,void *key,void *value,int32_t valuesize)
{
    char ipaddr[64]; int32_t i; FILE *fp = (FILE *)(long)args; struct iguana_peer *addr; struct iguana_iAddr *iA = value;
    if ( fp != 0 && iA != 0 && iA->numconnects > 0 && iA->lastconnect > time(NULL)-IGUANA_RECENTPEER )
    {
        for (i=0; i<coin->peers.numranked; i++)
            if ( (addr= coin->peers.ranked[i]) != 0 && addr->ipbits == iA->ipbits )
                break;
        if ( i == coin->peers.numranked )
        {
            expand_ipbits(ipaddr,iA->ipbits);
            fprintf(fp,"%s\n",ipaddr);
        }
    }
    return(0);
}

uint32_t iguana_updatemetrics(struct iguana_info *coin)
{
    char fname[512],tmpfname[512],oldfname[512]; int32_t i; struct iguana_peer *addr; FILE *fp;
    iguana_peermetrics(coin);
    sprintf(fname,"confs/%s_peers.txt",coin->symbol), OS_compatible_path(fname);
    sprintf(oldfname,"confs/%s_oldpeers.txt",coin->symbol), OS_compatible_path(oldfname);
    sprintf(tmpfname,"%s/%s/peers.txt",GLOBALTMPDIR,coin->symbol), OS_compatible_path(tmpfname);
    if ( (fp= fopen(tmpfname,"w")) != 0 )
    {
        for (i=0; i<coin->peers.numranked; i++)
            if ( (addr= coin->peers.ranked[i]) != 0 && addr->relayflag != 0 && strcmp(addr->ipaddr,"127.0.0.1") != 0 )
                fprintf(fp,"%s\n",addr->ipaddr);
        if ( ftell(fp) > OS_filesize(fname) )
        {
            printf("new peers.txt %ld vs (%s) %ld\n",ftell(fp),fname,(long)OS_filesize(fname));
            fclose(fp);
            OS_renamefile(fname,oldfname);
            OS_copyfile(tmpfname,fname,1);
        } else fclose(fp);
    }
    return((uint32_t)time(NULL));
}

void iguana_emitQ(struct iguana_info *coin,struct iguana_bundle *bp)
{
    struct iguana_helper *ptr;
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->bp = bp, ptr->hdrsi = bp->hdrsi;
    ptr->type = 'E';
    ptr->starttime = (uint32_t)time(NULL);
    //printf("%s EMIT.%d[%d] emitfinish.%u\n",coin->symbol,ptr->hdrsi,bp->n,bp->emitfinish);
    queue_enqueue("emitQ",&emitQ,&ptr->DL,0);
}

void iguana_mergeQ(struct iguana_info *coin,struct iguana_bundle *bp,struct iguana_bundle *nextbp)
{
    struct iguana_helper *ptr;
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->bp = bp, ptr->hdrsi = bp->hdrsi;
    ptr->nextbp = nextbp;
    ptr->type = 'M';
    ptr->starttime = (uint32_t)time(NULL);
    //printf("%s EMIT.%d[%d] emitfinish.%u\n",coin->symbol,ptr->hdrsi,bp->n,bp->emitfinish);
    queue_enqueue("helperQ",&helperQ,&ptr->DL,0);
}

void iguana_bundleQ(struct iguana_info *coin,struct iguana_bundle *bp,int32_t timelimit)
{
    struct iguana_helper *ptr;
    bp->queued = (uint32_t)time(NULL);
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->bp = bp, ptr->hdrsi = bp->hdrsi;
    ptr->type = 'B';
    ptr->starttime = (uint32_t)time(NULL);
    ptr->timelimit = timelimit;
    coin->numbundlesQ++;
    //printf("%s %p bundle.%d[%d] ht.%d emitfinish.%u\n",coin->symbol,bp,ptr->hdrsi,bp->n,bp->bundleheight,bp->emitfinish);
    queue_enqueue("bundlesQ",&bundlesQ,&ptr->DL,0);
}

void iguana_validateQ(struct iguana_info *coin,struct iguana_bundle *bp)
{
    struct iguana_helper *ptr;
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->bp = bp, ptr->hdrsi = bp->hdrsi;
    ptr->type = 'V';
    ptr->starttime = (uint32_t)time(NULL);
    ptr->timelimit = 0;
    printf("VALIDATE Q %s bundle.%d[%d] utxofinish.%u balancefinish.%u\n",coin->symbol,ptr->hdrsi,bp->n,bp->utxofinish,bp->balancefinish);
    queue_enqueue("validateQ",&validateQ,&ptr->DL,0);
}

void iguana_balancesQ(struct iguana_info *coin,struct iguana_bundle *bp)
{
    struct iguana_helper *ptr;
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->bp = bp, ptr->hdrsi = bp->hdrsi;
    ptr->type = 'B';
    ptr->starttime = (uint32_t)time(NULL);
    ptr->timelimit = 0;
    printf("BALANCES Q %s bundle.%d[%d] balances.%u balancefinish.%u\n",coin->symbol,ptr->hdrsi,bp->n,bp->utxofinish,bp->balancefinish);
    queue_enqueue("balancesQ",&balancesQ,&ptr->DL,0);
}

int32_t iguana_helpertask(FILE *fp,struct OS_memspace *mem,struct OS_memspace *memB,struct iguana_helper *ptr)
{
    struct iguana_info *coin; struct iguana_peer *addr; struct iguana_bundle *bp,*nextbp;
    addr = ptr->addr;
    if ( (coin= ptr->coin) != 0 )
    {
        if ( (bp= ptr->bp) != 0 )
        {
            if ( 0 && ptr->type == 'M' )
            {
                if ( (nextbp= ptr->nextbp) != 0 )
                {
                    bp->mergefinish = nextbp->mergefinish = (uint32_t)time(NULL);
                    if ( iguana_bundlemergeHT(coin,mem,memB,bp,nextbp,ptr->starttime) < 0 )
                        bp->mergefinish = nextbp->mergefinish = 0;
                }
            }
            else if ( ptr->type == 'B' )
            {
                iguana_bundleiters(coin,bp,ptr->timelimit);
            }
            else if ( ptr->type == 'E' )
            {
                if ( iguana_bundlesaveHT(coin,mem,memB,bp,ptr->starttime) == 0 )
                {
                    //fprintf(stderr,"emitQ coin.%p bp.[%d]\n",ptr->coin,bp->bundleheight);
                    bp->emitfinish = (uint32_t)time(NULL) + 1;
                    coin->numemitted++;
                } else bp->emitfinish = 0;
            }
        } else printf("no bundle in helperrequest\n");
    } else printf("no coin in helperrequest\n");
    return(0);
}

void iguana_balancecalc(struct iguana_info *coin,struct iguana_bundle *bp)
{
    uint32_t starttime;
    starttime = (uint32_t)time(NULL);
    if ( iguana_balancegen(coin,bp) < 0 )
    {
        printf("GENERATE BALANCES ERROR ht.%d\n",bp->bundleheight);
        exit(-1);
    }
    //iguana_coinflush(coin);
    bp->balancefinish = (uint32_t)time(NULL);
    printf("GENERATED BALANCES for ht.%d duration %d seconds\n",bp->bundleheight,bp->balancefinish - (uint32_t)starttime);
    iguana_validateQ(coin,bp);
}

void iguana_helper(void *arg)
{
    FILE *fp = 0; char fname[512],name[64],*helpername = 0; cJSON *argjson=0; int32_t flag,idle=0;
    struct iguana_helper *ptr; struct iguana_info *coin; struct OS_memspace MEM,*MEMB;
    if ( arg != 0 && (argjson= cJSON_Parse(arg)) != 0 )
        helpername = jstr(argjson,"name");
    if ( helpername == 0 )
    {
        sprintf(name,"helper.%d",rand());
        helpername = name;
    }
    sprintf(fname,"%s/%s",GLOBALTMPDIR,helpername);
    OS_compatible_path(fname);
    fp = fopen(fname,"wb");
    if ( argjson != 0 )
        free_json(argjson);
    memset(&MEM,0,sizeof(MEM));
    MEMB = mycalloc('b',IGUANA_MAXBUNDLESIZE,sizeof(*MEMB));
    while ( 1 )
    {
        //iguana_jsonQ();
        flag = 0;
        if ( (ptr= queue_dequeue(&validateQ,0)) != 0 )
        {
            if ( ptr->bp != 0 && ptr->coin != 0 )
                flag += iguana_bundlevalidate(ptr->coin,ptr->bp);
            else printf("helper validate missing param? %p %p\n",ptr->coin,ptr->bp);
            myfree(ptr,ptr->allocsize);
            flag++;
        }
        else if ( (ptr= queue_dequeue(&emitQ,0)) != 0 || (ptr= queue_dequeue(&helperQ,0)) != 0 )
        {
            if ( ptr->bp != 0 && (coin= ptr->coin) != 0 )
            {
                idle = 0;
                coin->helperdepth++;
                iguana_helpertask(fp,&MEM,MEMB,ptr);
                coin->helperdepth--;
                flag++;
            }
            myfree(ptr,ptr->allocsize);
        }
        else if ( (ptr= queue_dequeue(&bundlesQ,0)) != 0 )
        {
            idle = 0;
            if ( ptr->bp != 0 && ptr->coin != 0 )
                flag += iguana_bundleiters(ptr->coin,ptr->bp,ptr->timelimit);
            else printf("helper missing param? %p %p %u\n",ptr->coin,ptr->bp,ptr->timelimit);
            myfree(ptr,ptr->allocsize);
            flag++;
        }
        else if ( (ptr= queue_dequeue(&balancesQ,0)) != 0 )
        {
            flag++;
            if ( ptr->bp != 0 && ptr->coin != 0 )
                iguana_balancecalc(ptr->coin,ptr->bp);
            myfree(ptr,ptr->allocsize);
        }
        if ( flag == 0 )
            usleep(1000000);
        else usleep(100000);
    }
}

void iguana_coinflush(struct iguana_info *coin)
{
    int32_t hdrsi; struct iguana_bundle *bp; char fname[1024],fname2[1024]; FILE *fp,*fp2=0;
    for (hdrsi=0; hdrsi<coin->bundlescount; hdrsi++)
    {
        if ( (bp= coin->bundles[hdrsi]) != 0 && bp->dirty != 0 && time(NULL) > bp->dirty+60 && bp->ramchain.H.data != 0 && bp->ramchain.A != 0 && bp->ramchain.Uextras != 0 )
        {
            sprintf(fname,"accounts/%s/debits.%d",coin->symbol,bp->bundleheight);
            sprintf(fname2,"accounts/%s/lastspends.%d",coin->symbol,bp->bundleheight);
            printf("save (%s) and (%s) %p %p\n",fname,fname2,bp,bp->ramchain.H.data);//,bp->ramchain.H.data->numpkinds,bp->ramchain.H.data->numunspents);
            if ( (fp= fopen(fname,"wb")) != 0 && (fp2= fopen(fname2,"wb")) != 0 )
            {
                if ( fwrite(bp->ramchain.A,sizeof(*bp->ramchain.A),bp->ramchain.H.data->numpkinds,fp) == bp->ramchain.H.data->numpkinds )
                {
                    if ( fwrite(bp->ramchain.Uextras,sizeof(*bp->ramchain.Uextras),bp->ramchain.H.data->numunspents,fp2) == bp->ramchain.H.data->numunspents )
                    {
                        bp->dirty = 0;
                        printf("saved (%s) and (%s)\n",fname,fname2);
                    }
                }
                fclose(fp), fclose(fp2);
            }
            else if ( fp != 0 )
                fclose(fp);
        }
    }
}

void iguana_coinloop(void *arg)
{
    struct iguana_info *coin,**coins = arg;
    struct iguana_bundle *bp; int32_t flag,i,n,bundlei; bits256 zero; char str[1024];
    uint32_t now;
    n = (int32_t)(long)coins[0];
    coins++;
    printf("begin coinloop[%d]\n",n);
    for (i=0; i<n; i++)
    {
        if ( (coin= coins[i]) != 0 && coin->started == 0 )
        {
            iguana_coinstart(coin,coin->initialheight,coin->mapflags);
            printf("init.(%s) maxpeers.%d maxrecvcache.%s services.%llx MAXMEM.%s polltimeout.%d\n",coin->symbol,coin->MAXPEERS,mbstr(str,coin->MAXRECVCACHE),(long long)coin->myservices,mbstr(str,coin->MAXMEM),coin->polltimeout);
            coin->started = coin;
            coin->chain->minconfirms = coin->minconfirms;
        }
    }
    coin = coins[0];
    iguana_rwiAddrind(coin,0,0,0);
    iguana_possible_peer(coin,"127.0.0.1");
    memset(zero.bytes,0,sizeof(zero));
    if ( (bp= iguana_bundlecreate(coin,&bundlei,0,*(bits256 *)coin->chain->genesis_hashdata,zero,1)) != 0 )
        bp->bundleheight = 0;
    while ( 1 )
    {
        //fprintf(stderr,"iter\n");
        flag = 0;
        for (i=0; i<n; i++)
        {
            if ( (coin= coins[i]) != 0 )
            {
                now = (uint32_t)time(NULL);
                if ( coin->active != 0 )
                {
                    if ( coin->isRT == 0 && now > coin->startutc+600 && coin->blocksrecv >= (coin->longestchain/coin->chain->bundlesize)*coin->chain->bundlesize && coin->blocks.hwmchain.height >= coin->longestchain-30 )
                    {
                        iguana_coinflush(coin);
                        fprintf(stderr,">>>>>>> %s isRT blockrecv.%d vs longest.%d\n",coin->symbol,coin->blocksrecv,coin->longestchain);
                        coin->isRT = 1;
                        if ( coin->polltimeout > 100 )
                            coin->polltimeout = 100;
                        coin->MAXPEERS = 8;
                    }
                    if ( coin->isRT != 0 )
                        iguana_coinflush(coin);
                    if ( coin->bindsock >= 0 )
                    {
                        if ( coin->peers.numranked < 8 && now > coin->lastpossible+60 )
                        {
                            //fprintf(stderr,"possible\n");
                            coin->lastpossible = iguana_possible_peer(coin,0); // tries to connect to new peers
                        }
                    }
                    else
                    {
                        if ( coin->peers.numranked != 0 && coin->peers.numranked < (coin->MAXPEERS>>1) && now > coin->lastpossible )
                        {
                            //fprintf(stderr,"possible\n");
                            coin->lastpossible = iguana_possible_peer(coin,0); // tries to connect to new peers
                        }
                    }
                    if ( now > coin->peers.lastmetrics+6 )
                    {
                        //fprintf(stderr,"metrics\n");
                        coin->peers.lastmetrics = iguana_updatemetrics(coin); // ranks peers
                    }
                    //fprintf(stderr,"call stats\n");
                    iguana_bundlestats(coin,str);
                    //fprintf(stderr,"call process\n");
                    flag += iguana_processrecv(coin);
                    if ( coin->longestchain+10000 > coin->blocks.maxbits )
                        iguana_recvalloc(coin,coin->longestchain + 100000);
                }
            }
        }
        if ( flag == 0 )
            usleep(10000);
    }
}

void iguana_coinargs(char *symbol,int64_t *maxrecvcachep,int32_t *minconfirmsp,int32_t *maxpeersp,int32_t *initialheightp,uint64_t *servicesp,int32_t *maxpendingp,int32_t *maxbundlesp,cJSON *json)
{
    if ( (*maxrecvcachep= j64bits(json,"maxrecvcache")) != 0 )
        *maxrecvcachep *= 1024 * 1024 * 1024L;
    *minconfirmsp = juint(json,"minconfirms");
    *maxpeersp = juint(json,"maxpeers");
    *maxpendingp = juint(json,"maxpending");
    *maxbundlesp = juint(json,"maxbundles");
    if ( (*initialheightp= juint(json,"initialheight")) == 0 )
        *initialheightp = (strcmp(symbol,"BTC") == 0) ? 400000 : 100000;
    *servicesp = j64bits(json,"services");
}

struct iguana_info *iguana_setcoin(char *symbol,void *launched,int32_t maxpeers,int64_t maxrecvcache,uint64_t services,int32_t initialheight,int32_t maphash,int32_t minconfirms,int32_t maxpending,int32_t maxbundles,cJSON *json)
{
    struct iguana_chain *iguana_createchain(cJSON *json);
    struct iguana_info *coin; int32_t j,m,mapflags; char dirname[512]; cJSON *peers;
    mapflags = IGUANA_MAPRECVDATA | maphash*IGUANA_MAPTXIDITEMS | maphash*IGUANA_MAPPKITEMS | maphash*IGUANA_MAPBLOCKITEMS | maphash*IGUANA_MAPPEERITEMS;
    coin = iguana_coinadd(symbol,json);
    coin->launched = launched;
    if ( (coin->MAXPEERS= maxpeers) <= 0 )
        coin->MAXPEERS = (strcmp(symbol,"BTC") == 0) ? 128 : 64;
    if ( (coin->MAXRECVCACHE= maxrecvcache) == 0 )
        coin->MAXRECVCACHE = IGUANA_MAXRECVCACHE;
    if ( (coin->MAXPENDING= maxpending) <= 0 )
        coin->MAXPENDING = (strcmp(symbol,"BTC") == 0) ? _IGUANA_MAXPENDING : _IGUANA_MAXPENDING;
    if ( (coin->MAXBUNDLES= maxbundles) <= 0 )
        coin->MAXBUNDLES = (strcmp(symbol,"BTC") == 0) ? IGUANA_MAXPENDBUNDLES : IGUANA_MAXPENDBUNDLES * 3;
    coin->myservices = services;
    sprintf(dirname,"accounts/%s",symbol), OS_ensure_directory(dirname);
    sprintf(dirname,"DB/%s",symbol), OS_ensure_directory(dirname);
    sprintf(dirname,"DB/%s/spends",symbol), OS_ensure_directory(dirname);
    sprintf(dirname,"DB/%s/vouts",symbol), OS_ensure_directory(dirname);
    sprintf(dirname,"purgeable/%s",symbol), OS_ensure_directory(dirname);
    sprintf(dirname,"%s/%s",GLOBALTMPDIR,symbol), OS_ensure_directory(dirname);
    coin->initialheight = initialheight;
    coin->mapflags = mapflags;
    coin->MAXMEM = juint(json,"RAM");
    if ( coin->MAXMEM == 0 )
        coin->MAXMEM = IGUANA_DEFAULTRAM;
    coin->MAXMEM *= (1024 * 1024 * 1024);
    if ( (coin->polltimeout= juint(json,"poll")) <= 0 )
        coin->polltimeout = 10;
    char str[65]; printf("MAXMEM.%s\n",mbstr(str,coin->MAXMEM));
    coin->active = juint(json,"active");
    if ( (coin->minconfirms = minconfirms) == 0 )
        coin->minconfirms = (strcmp(symbol,"BTC") == 0) ? 3 : 10;
    if ( coin->chain == 0 && (coin->chain= iguana_createchain(json)) == 0 )
    {
        printf("cant initialize chain.(%s)\n",jstr(json,0));
        return(0);
    } else iguana_chainparms(coin->chain,json);
    if ( (peers= jarray(&m,json,"peers")) != 0 )
    {
        for (j=0; j<m; j++)
        {
            printf("%s ",jstr(jitem(peers,j),0));
            iguana_possible_peer(coin,jstr(jitem(peers,j),0));
        }
        printf("addnodes.%d\n",m);
    }
    return(coin);
}

int32_t iguana_launchcoin(char *symbol,cJSON *json)
{
    int32_t maxpeers,maphash,initialheight,minconfirms,maxpending,maxbundles;
    int64_t maxrecvcache; uint64_t services; struct iguana_info **coins,*coin;
    if ( symbol == 0 )
        return(-1);
    if ( (coin= iguana_coinadd(symbol,json)) == 0 )
        return(-1);
    if ( coin->launched == 0 )
    {
        if ( juint(json,"GBavail") < 8 )
            maphash = IGUANA_MAPHASHTABLES;
        else maphash = 0;
        iguana_coinargs(symbol,&maxrecvcache,&minconfirms,&maxpeers,&initialheight,&services,&maxpending,&maxbundles,json);
        coins = mycalloc('A',1+1,sizeof(*coins));
        if ( (coin= iguana_setcoin(symbol,coins,maxpeers,maxrecvcache,services,initialheight,maphash,minconfirms,maxpending,maxbundles,json)) != 0 )
        {
            coins[0] = (void *)((long)1);
            coins[1] = coin;
            printf("launch coinloop for.%s services.%llx\n",coin->symbol,(long long)services);
            iguana_launch(coin,"iguana_coinloop",iguana_coinloop,coins,IGUANA_PERMTHREAD);
            coin->active = 1;
            return(1);
        }
        else
        {
            myfree(coins,sizeof(*coins) * 2);
            return(-1);
        }
    }
    return(0);
}

void iguana_coins(void *arg)
{
    struct iguana_info **coins,*coin; char *jsonstr,*symbol; cJSON *array,*item,*json;
    int32_t i,n,maxpeers,maphash,initialheight,minconfirms,maxpending,maxbundles;
    int64_t maxrecvcache; uint64_t services; struct vin_info V;
    memset(&V,0,sizeof(V));
    if ( (jsonstr= arg) != 0 && (json= cJSON_Parse(jsonstr)) != 0 )
    {
        if ( (array= jarray(&n,json,"coins")) == 0 )
        {
            if ( (symbol= jstr(json,"coin")) != 0 && strncmp(symbol,"BTC",3) == 0 )
            {
                coins = mycalloc('A',1+1,sizeof(*coins));
                coins[1] = iguana_setcoin(symbol,coins,0,0,0,0,0,0,0,0,json);
                _iguana_calcrmd160(coins[1],&V);
                coins[0] = (void *)((long)1);
                iguana_coinloop(coins);
            } else printf("no coins[] array in JSON.(%s) only BTCD and BTC can be quicklaunched\n",jsonstr);
            free_json(json);
            return;
        }
        coins = mycalloc('A',n+1,sizeof(*coins));
        if ( juint(json,"GBavail") < 8 )
            maphash = IGUANA_MAPHASHTABLES;
        else maphash = 0;
        for (i=0; i<n; i++)
        {
            item = jitem(array,i);
            if ( (symbol= jstr(item,"name")) == 0 || strlen(symbol) > 8 )
            {
                printf("skip strange coin.(%s)\n",symbol);
                continue;
            }
            iguana_coinargs(symbol,&maxrecvcache,&minconfirms,&maxpeers,&initialheight,&services,&maxpending,&maxbundles,item);
            coins[1 + i] = coin = iguana_setcoin(symbol,coins,maxpeers,maxrecvcache,services,initialheight,maphash,minconfirms,maxpending,maxbundles,item);
        }
        coins[0] = (void *)((long)n);
        iguana_coinloop(coins);
    }
}

char *busdata_sync(uint32_t *noncep,char *jsonstr,char *broadcastmode,char *destNXTaddr)
{
    printf("busdata_sync.(%s)\n",jsonstr);
    return(0);
}

