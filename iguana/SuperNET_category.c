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

queue_t *category_Q(bits256 categoryhash,bits256 subhash)
{
    struct category_info *cat,*sub; queue_t *Q = 0;
    HASH_FIND(hh,Categories,categoryhash.bytes,sizeof(categoryhash),cat);
    if ( cat != 0 )
    {
        if ( bits256_nonz(subhash) > 0 && memcmp(GENESIS_PUBKEY.bytes,subhash.bytes,sizeof(subhash)) != 0 )
        {
            HASH_FIND(hh,cat->sub,subhash.bytes,sizeof(subhash),sub);
            if ( sub != 0 )
                Q = &sub->Q;
        } else Q = &cat->Q;
    }
    return(Q);
}

struct category_msg *category_gethexmsg(struct supernet_info *myinfo,bits256 categoryhash,bits256 subhash)
{
    queue_t *Q;
    if ( (Q= category_Q(categoryhash,subhash)) != 0 )
        return(queue_dequeue(Q,0));
    else return(0);
}

void category_posthexmsg(struct supernet_info *myinfo,bits256 categoryhash,bits256 subhash,char *hexmsg,struct tai now)
{
    int32_t len; struct category_msg *m; queue_t *Q = 0;
    if ( (Q= category_Q(categoryhash,subhash)) != 0 )
    {
        len = (int32_t)strlen(hexmsg) >> 1;
        m = calloc(1,sizeof(*m) + len);
        m->t = now, m->len = len;
        decode_hex(m->msg,m->len,hexmsg);
        queue_enqueue("categoryQ",Q,&m->DL,0);
        return;
    }
    char str[65]; printf("no subscription for category.(%s) %llx\n",bits256_str(str,categoryhash),(long long)subhash.txid);
}

void *category_sub(struct supernet_info *myinfo,bits256 categoryhash,bits256 subhash)
{
    struct category_info *cat,*sub; bits256 hash;
    HASH_FIND(hh,Categories,categoryhash.bytes,sizeof(categoryhash),cat);
    if ( cat == 0 )
    {
        cat = mycalloc('c',1,sizeof(*cat));
        cat->hash = hash = categoryhash;
        HASH_ADD(hh,Categories,hash,sizeof(hash),cat);
    }
    if ( bits256_nonz(subhash) > 0 && memcmp(GENESIS_PUBKEY.bytes,subhash.bytes,sizeof(subhash)) != 0 && cat != 0 )
    {
        HASH_FIND(hh,cat->sub,subhash.bytes,sizeof(subhash),sub);
        if ( sub == 0 )
        {
            sub = mycalloc('c',1,sizeof(*sub));
            sub->hash = hash = subhash;
            HASH_ADD(hh,cat->sub,hash,sizeof(hash),sub);
        }
    }
    return(cat);
}

int32_t category_peer(struct supernet_info *myinfo,struct iguana_peer *addr,bits256 category,bits256 subhash)
{
    return(1);
}

int32_t category_plaintext(struct supernet_info *myinfo,bits256 category,bits256 subhash,int32_t plaintext)
{
    return(plaintext);
}

int32_t category_maxdelay(struct supernet_info *myinfo,bits256 category,bits256 subhash,int32_t maxdelay)
{
    return(maxdelay);
}

int32_t category_broadcastflag(struct supernet_info *myinfo,bits256 category,bits256 subhash,int32_t broadcastflag)
{
    if ( broadcastflag < 1 )
        broadcastflag = 1;
    else if ( broadcastflag > SUPERNET_MAXHOPS )
        broadcastflag = SUPERNET_MAXHOPS;
    return(broadcastflag);
}

char *SuperNET_categorymulticast(struct supernet_info *myinfo,int32_t surveyflag,bits256 category,bits256 subhash,char *message,int32_t maxdelay,int32_t broadcastflag,int32_t plaintext)
{
    char *hexmsg,*retstr; int32_t len;
    len = (int32_t)strlen(message);
    if ( is_hexstr(message,len) == 0 )
    {
        hexmsg = malloc((len << 1) + 1);
        init_hexbytes_noT(hexmsg,(uint8_t *)message,len+1);
    } else hexmsg = message;
    plaintext = category_plaintext(myinfo,category,subhash,plaintext);
    broadcastflag = category_broadcastflag(myinfo,category,subhash,broadcastflag);
    maxdelay = category_maxdelay(myinfo,category,subhash,maxdelay);
    retstr = SuperNET_DHTsend(myinfo,0,category,subhash,hexmsg,maxdelay,broadcastflag,plaintext);
    if ( hexmsg != message)
        free(hexmsg);
    return(retstr);
}
