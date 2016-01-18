
#define IGUANA_ARGS struct supernet_info *myinfo,struct iguana_info *coin,cJSON *json,char *remoteaddr
#define IGUANA_CFUNC0(agent,name) char *agent ## _ ## name(IGUANA_ARGS)
#define IGUANA_CFUNC_S(agent,name,str) char *agent ## _ ## name(IGUANA_ARGS,char *str)
#define IGUANA_CFUNC_I(agent,name,val) char *agent ## _ ## name(IGUANA_ARGS,int32_t val)
#define IGUANA_CFUNC_SA(agent,name,str,array) char *agent ## _ ## name(IGUANA_ARGS,char *str,cJSON *array)
#define IGUANA_CFUNC_AA(agent,name,array,array2) char *agent ## _ ## name(IGUANA_ARGS,cJSON *array,cJSON *array2)
#define IGUANA_CFUNC_SAA(agent,name,str,array,array2) char *agent ## _ ## name(IGUANA_ARGS,char *str,cJSON *array,cJSON *array2)
#define IGUANA_CFUNC_IA(agent,name,val,array) char *agent ## _ ## name(IGUANA_ARGS,int32_t val,cJSON *array)
#define IGUANA_CFUNC_IAS(agent,name,val,array,str) char *agent ## _ ## name(IGUANA_ARGS,int32_t val,cJSON *array,char *str)
#define IGUANA_CFUNC_II(agent,name,val,val2) char *agent ## _ ## name(IGUANA_ARGS,int32_t val,int32_t val2)
#define IGUANA_CFUNC_III(agent,name,val,val2,val3) char *agent ## _ ## name(IGUANA_ARGS,int32_t val,int32_t val2,int32_t val3)
#define IGUANA_CFUNC_SIII(agent,name,str,val,val2,val3) char *agent ## _ ## name(IGUANA_ARGS,char *str,int32_t val,int32_t val2,int32_t val3)
#define IGUANA_CFUNC_IIA(agent,name,val,val2,array) char *agent ## _ ## name(IGUANA_ARGS,int32_t val,int32_t val2,cJSON *array)
#define IGUANA_CFUNC_SS(agent,name,str,str2) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2)
#define IGUANA_CFUNC_SSI(agent,name,str,str2,val) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,int32_t val)
#define IGUANA_CFUNC_SSH(agent,name,str,str2,hash) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,bits256 hash)
#define IGUANA_CFUNC_SSHI(agent,name,str,str2,hash,val) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,bits256 hash,int32_t val)
#define IGUANA_CFUNC_SSHII(agent,name,str,str2,hash,val,val2) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,bits256 hash,int32_t val,int32_t val2)
#define IGUANA_CFUNC_SSS(agent,name,str,str2,str3) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,char *str3)
#define IGUANA_CFUNC_SI(agent,name,str,val) char *agent ## _ ## name(IGUANA_ARGS,char *str,int32_t val)
#define IGUANA_CFUNC_SII(agent,name,str,val,val2) char *agent ## _ ## name(IGUANA_ARGS,char *str,int32_t val,int32_t val2)
#define IGUANA_CFUNC_HI(agent,name,hash,val) char *agent ## _ ## name(IGUANA_ARGS,bits256 hash,int32_t val)
#define IGUANA_CFUNC_HII(agent,name,hash,val,val2) char *agent ## _ ## name(IGUANA_ARGS,bits256 hash,int32_t val,int32_t val2)
#define IGUANA_CFUNC_D(agent,name,val) char *agent ## _ ## name(IGUANA_ARGS,double val)
#define IGUANA_CFUNC_SSDIS(agent,name,str,str2,amount,val,str3) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,double amount,int32_t val,char *str3)
#define IGUANA_CFUNC_SSDISS(agent,name,str,str2,amount,val,str3,str4) char *agent ## _ ## name(IGUANA_ARGS,char *str,char *str2,double amount,int32_t val,char *str3,char *str4)
#define IGUANA_CFUNC_SAIS(agent,name,str,array,val,str2) char *agent ## _ ## name(IGUANA_ARGS,char *str,cJSON *array,int32_t val,char *str2)
#define IGUANA_CFUNC_SDSS(agent,name,str,amount,str2,str3) char *agent ## _ ## name(IGUANA_ARGS,char *str,double amount,char *str2,char *str3)

// API functions
#define ZERO_ARGS IGUANA_CFUNC0
#define INT_ARG IGUANA_CFUNC_I
#define TWO_INTS IGUANA_CFUNC_II
#define STRING_ARG IGUANA_CFUNC_S
#define TWO_STRINGS IGUANA_CFUNC_SS
#define THREE_STRINGS IGUANA_CFUNC_SSS
#define STRING_AND_INT IGUANA_CFUNC_SI
#define STRING_AND_TWOINTS IGUANA_CFUNC_SII
#define HASH_AND_INT IGUANA_CFUNC_HI
#define HASH_AND_TWOINTS IGUANA_CFUNC_HII
#define DOUBLE_ARG IGUANA_CFUNC_D
#define STRING_AND_ARRAY IGUANA_CFUNC_SA
#define STRING_AND_TWOARRAYS IGUANA_CFUNC_SAA
#define TWO_ARRAYS IGUANA_CFUNC_AA
#define INT_AND_ARRAY IGUANA_CFUNC_IA
#define INT_ARRAY_STRING IGUANA_CFUNC_IAS
#define SS_D_I_S IGUANA_CFUNC_SSDIS
#define SS_D_I_SS IGUANA_CFUNC_SSDISS
#define S_A_I_S IGUANA_CFUNC_SAIS
#define S_D_SS IGUANA_CFUNC_SDSS
#define TWOINTS_AND_ARRAY IGUANA_CFUNC_IIA
#define STRING_AND_THREEINTS IGUANA_CFUNC_SIII
#define TWOSTRINGS_AND_INT IGUANA_CFUNC_SSI
#define TWOSTRINGS_AND_HASH IGUANA_CFUNC_SSH
#define TWOSTRINGS_AND_HASH_AND_TWOINTS IGUANA_CFUNC_SSHII
#define THREE_INTS IGUANA_CFUNC_III
