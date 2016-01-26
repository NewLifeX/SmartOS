#include "RSA.h"

#define KeyLength 1024

#define __RSA_CONFIG_DECRYPT    1
//#undef  __RSA_CONFIG_DECRYPT

#define __RSA_LIB_MAIN_V     0x01
#define __RSA_LIB_SUPPORT_DE     0x80

#ifdef __RSA_CONFIG_DECRYPT
	#define __RSA_LIB_V     (__RSA_LIB_MAIN_V | __RSA_LIB_SUPPORT_DE)
#else
	#define __RSA_LIB_V     __RSA_LIB_MAIN_V
#endif

#define RSA_BITS        1024UL

#define RSA_CONSTANT_SHIFT      32      /* divides agrithm every period bits */
#define RSA_INTS                         RSA_BITS/32
#define RSA_MUL_BUF_CNT                 (2 * RSA_INTS + 1)  /* rsa 1024bits 64 bytes */
#define RSA_DIVIDE_BUF_CNT              (2 * RSA_INTS + 1)   /* rsa 1024bits 65 bytes */
#define RSA_MAX_DES_BUF_CNT             (2 * RSA_INTS + 1)    /* des buf cnt */


uint32_t __RSA_MUL_BUF[RSA_MUL_BUF_CNT]; // 64bytes
uint32_t __RSA_DIVIDE_BUF[RSA_DIVIDE_BUF_CNT]; //  65bytes


typedef struct
{
	uint32_t *addr;      /* data start address and LSB first */
	uint8_t len;        /* data length */
}rsa_data_TypeDef;


struct __RSA_ENCRY_STRUC{
	rsa_data_TypeDef *temp_x;
	rsa_data_TypeDef *temp_des;
	rsa_data_TypeDef *temp_u;
	rsa_data_TypeDef *mes;
	rsa_data_TypeDef *n0b;
	rsa_data_TypeDef *mod;

	rsa_data_TypeDef *pb_key;

#ifdef     __RSA_CONFIG_DECRYPT
	rsa_data_TypeDef *pb_mod;

	rsa_data_TypeDef *pb_n0b;

	rsa_data_TypeDef *de_mod;
	rsa_data_TypeDef *de_pr_key;
	rsa_data_TypeDef *de_n0b;
#endif
	uint8_t ver;
};

rsa_data_TypeDef __temp_x_s, __temp_des_s, __temp_u_s, __n0b_s,__mes_s,__mod_s;
rsa_data_TypeDef __pb_key_s;
#ifdef     __RSA_CONFIG_DECRYPT
	rsa_data_TypeDef  __pb_mod_s, __pb_n0b_s,__de_mod_s, __de_pr_key_s, __de_n0b_s ;
#endif

struct __RSA_ENCRY_STRUC __RSA_ENCRY_S;

#define __RSA_MAX_TEMP_X_DATA_CNT     (RSA_INTS + 2)  /* must big Real data count */
#define __RSA_MAX_TEMP_DES_DATA_CNT    (RSA_INTS*2 + 1)
#define __RSA_MAX_TEMP_U_DATA_CNT       (RSA_INTS + 1)
#define __RSA_MAX_N0B_DATA_CNT          1
#define __RSA_MAX_MES_DATA_CNT          (RSA_INTS*2 + 1)
#define __RSA_MAX_MOD_DATA_CNT          RSA_INTS

#define __RSA_MAX_PB_KEY_DATA_CNT       RSA_INTS
#define __RSA_MAX_PB_MOD_DATA_CNT          RSA_INTS
#define __RSA_MAX_PB_N0B_DATA_CNT          1

#define __RSA_MAX_DE_PR_KEY_DATA_CNT       RSA_INTS
#define __RSA_MAX_DE_MOD_DATA_CNT       RSA_INTS
#define __RSA_MAX_DE_N0B_DATA_CNT        1

uint32_t __temp_x_data[__RSA_MAX_TEMP_X_DATA_CNT];
uint32_t __temp_des_data[__RSA_MAX_TEMP_DES_DATA_CNT];
//uint32_t __temp_u_data[__RSA_MAX_TEMP_U_DATA_CNT];
uint32_t __n0b_data[__RSA_MAX_N0B_DATA_CNT];
uint32_t __mes_data[__RSA_MAX_MES_DATA_CNT];
uint32_t __mod_data[__RSA_MAX_MOD_DATA_CNT];

uint32_t __pb_key_data[__RSA_MAX_PB_KEY_DATA_CNT];
#ifdef     __RSA_CONFIG_DECRYPT
	uint32_t __pb_mod_data[__RSA_MAX_PB_MOD_DATA_CNT];
	uint32_t __pb_n0b_data[__RSA_MAX_PB_N0B_DATA_CNT];

	uint32_t __de_mod_data[__RSA_MAX_DE_MOD_DATA_CNT];
	uint32_t __de_pr_key_data[__RSA_MAX_DE_PR_KEY_DATA_CNT];
	uint32_t __de_n0b_data[__RSA_MAX_DE_N0B_DATA_CNT];
#endif
#define RSA_SOS_BITS_S RSA_INTS
#define __RSA_WORDS_W RSA_INTS
#define __RSA_MON_PRO_MAX_CARRY  (RSA_INTS*2 + 1)
#define __RSA_MON_PRO_SINGLE_MAX_CARRY  (RSA_INTS*2 + 1)
#define __RSA_MON_PRO_DOU_MAX_CARRY  (RSA_INTS*2 + 1)


static inline  void __rsa_memset(uint32_t *des, uint32_t content, uint8_t length);
static void rsa_divide(uint32_t *des, uint8_t *des_len, uint32_t *sour, uint8_t *sour_len);
static inline void __rsa_mon_pro (rsa_data_TypeDef *x,rsa_data_TypeDef *a);
static inline void __rsa_mon_pro_single(rsa_data_TypeDef *x);
static inline void __rsa_mon_pro_double(rsa_data_TypeDef *sour_x);
static inline void __rsa_mon_exp_f(rsa_data_TypeDef *x,rsa_data_TypeDef *a, rsa_data_TypeDef *pb_key);


static inline  void __rsa_clr(rsa_data_TypeDef *des)
{
	uint8_t i;
	for( i = 0; i < des->len; i++)
		(des->addr)[i] = 0x0;
	des->len = 0x1;
}

static inline  void __rsa_memset(uint32_t *des, uint32_t content, uint8_t length)
{
	uint8_t i;

	for( i = 0; i < length ; i++)
		des[i] = content;
}

uint8_t rsa_struc_init(void)
{
	rsa_data_TypeDef  *n0b,*temp_des,*temp_u,*temp_x,*mes,*mod;
	rsa_data_TypeDef *pb_key ;

#ifdef __RSA_CONFIG_DECRYPT
	rsa_data_TypeDef  *pb_mod, *pb_n0b, *de_mod, *de_pr_key, *de_n0b;
#endif
	/*get pointer  Global variable */

	__RSA_ENCRY_S.temp_x =  &__temp_x_s;
	__RSA_ENCRY_S.temp_des =  &__temp_des_s;
	__RSA_ENCRY_S.temp_u =  &__temp_u_s;
	__RSA_ENCRY_S.mes =  &__mes_s;
	__RSA_ENCRY_S.n0b =  &__n0b_s;
	__RSA_ENCRY_S.mod =  &__mod_s;

	__RSA_ENCRY_S.pb_key =  &__pb_key_s;

 #ifdef     __RSA_CONFIG_DECRYPT
	__RSA_ENCRY_S.pb_mod =  &__pb_mod_s;
	__RSA_ENCRY_S.pb_n0b =  &__pb_n0b_s;

	__RSA_ENCRY_S.de_mod =  &__de_mod_s;
	__RSA_ENCRY_S.de_pr_key =  &__de_pr_key_s;
	__RSA_ENCRY_S.de_n0b =  &__de_n0b_s;
#endif
						/* init */
	temp_x = &__temp_x_s;   /* temp_x */
	temp_x->addr = __temp_x_data;
	temp_x->len = __RSA_MAX_TEMP_X_DATA_CNT;
	__rsa_clr(temp_x);

	temp_des = &__temp_des_s;   /* temp_des */
	temp_des->addr = __temp_des_data;
	temp_des->len = __RSA_MAX_TEMP_DES_DATA_CNT;
	__rsa_clr(temp_des);

	temp_u = &__temp_u_s;  /* temp_u*/
	//temp_u->addr = __temp_u_data;
	 temp_u->addr = __RSA_MUL_BUF;
	 temp_u->len = RSA_MUL_BUF_CNT;
	// temp_u->len = __RSA_MAX_TEMP_U_DATA_CNT;
	__rsa_clr(temp_u);

	n0b = &__n0b_s;   /* n0b*/
	n0b->addr = __n0b_data;
	n0b->len = __RSA_MAX_N0B_DATA_CNT;
	__rsa_clr(n0b);

	mes = &__mes_s;   /* mes*/
	mes->addr = __mes_data;
	mes->len = __RSA_MAX_MES_DATA_CNT;
	__rsa_clr(mes);

	mod = &__mod_s;   /* mod*/
	mod->addr = __mod_data;
	mod->len = __RSA_MAX_MOD_DATA_CNT;
	__rsa_clr(mod);

/****************encrtpt structure *********/

	pb_key = &__pb_key_s;   /* public key*/
	pb_key->addr = __pb_key_data;
	pb_key->len = __RSA_MAX_PB_KEY_DATA_CNT;
	__rsa_clr(pb_key);

#ifdef     __RSA_CONFIG_DECRYPT
	pb_mod = &__pb_mod_s;   /* Mod */
	pb_mod->addr = __pb_mod_data;
	pb_mod->len = __RSA_MAX_PB_MOD_DATA_CNT;
	__rsa_clr(pb_mod);

	pb_n0b = &__pb_n0b_s;   /* n0b*/
	pb_n0b->addr = __pb_n0b_data;
	pb_n0b->len = __RSA_MAX_PB_N0B_DATA_CNT;
	__rsa_clr(pb_n0b);

/****************decrtpt structure *********/
	de_mod = &__de_mod_s;   /* de_mod*/
	de_mod->addr = __de_mod_data;
	de_mod->len = __RSA_MAX_DE_MOD_DATA_CNT;
	__rsa_clr(de_mod);

	de_pr_key = &__de_pr_key_s;   /* de_mod*/
	de_pr_key->addr = __de_pr_key_data;
	de_pr_key->len = __RSA_MAX_DE_PR_KEY_DATA_CNT;
	__rsa_clr(de_pr_key);

	de_n0b = &__de_n0b_s;   /* de_mod*/
	de_n0b->addr = __de_n0b_data;
	de_n0b->len = __RSA_MAX_DE_N0B_DATA_CNT;
	__rsa_clr(de_n0b);
#endif
	/* finshed and set flag */
	__RSA_ENCRY_S .ver = __RSA_LIB_V;/* down */

	return 0;
}


inline static void __rsa_n0_inver(uint32_t *data)
{
	uint8_t i;
	uint64_t temp ,temp1, temp2,n,t;

	n = (uint64_t)*data;
	t = 1;

	for( i = 2; i <= 32; i++){
		temp1 = ((uint64_t)1) << i;
		temp = n % temp1;
		temp2 = t % temp1;
		temp = temp *temp2;
		temp = temp % temp1;
		if(temp >= temp1/2 ){
			t = t + temp1/2;
		}
	}
	n =  (uint64_t)0x100000000 - t;
	*data = (uint32_t)n;
}

uint8_t rsa_encrypt_init(uint32_t *p_mod, uint8_t mod_len, uint32_t *p_pb_key, uint8_t pb_key_len)
{
       uint8_t i;

#ifdef     __RSA_CONFIG_DECRYPT

        rsa_data_TypeDef  *pb_key,*pb_n0b;
       rsa_data_TypeDef *pb_mod;

       /*get pointer  Global variable */
       pb_mod = __RSA_ENCRY_S.pb_mod;
       pb_key = __RSA_ENCRY_S.pb_key;
       pb_n0b = __RSA_ENCRY_S.pb_n0b;

        /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < pb_mod->len; i++)
                pb_mod->addr[i] = 0x0;

        /* get new value*/
        pb_mod->len = mod_len;
        for( i = 0; i < mod_len; i++)
                pb_mod->addr[i] = p_mod[i];

         /* clr pubic key len,prevent program run in and reinitial */
        for( i = 0; i < pb_key->len; i++)
                pb_key->addr[i] = 0x0;

        /* get new value*/
        pb_key->len = pb_key_len;
        for( i = 0; i < pb_key_len; i++)
                pb_key->addr[i] = p_pb_key[i];

        /* get n0' data */
        pb_n0b->addr[0] = pb_mod->addr[0];
        __rsa_n0_inver(pb_n0b->addr);

#else

       rsa_data_TypeDef  *pb_key,*n0b;
       rsa_data_TypeDef *mod;

       /*get pointer  Global variable */
       mod = __RSA_ENCRY_S.mod;
       pb_key = __RSA_ENCRY_S.pb_key;
       n0b = __RSA_ENCRY_S.n0b;

        /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < mod->len; i++)
                mod->addr[i] = 0x0;

        /* get new value*/
        mod->len = mod_len;
        for( i = 0; i < mod->len; i++)
                mod->addr[i] = p_mod[i];

         /* clr pubic key len,prevent program run in and reinitial */
        for( i = 0; i < pb_key->len; i++)
                pb_key->addr[i] = 0x0;

        /* get new value*/
        pb_key->len = pb_key_len;
        for( i = 0; i < pb_key_len; i++)
                pb_key->addr[i] = p_pb_key[i];

        /* get n0' data */
         n0b->addr[0] = mod->addr[0];
        __rsa_n0_inver(n0b->addr);

#endif
        return 0;/* pass */
}

#ifdef     __RSA_CONFIG_DECRYPT
uint8_t rsa_decrypt_init(uint32_t *p_mod, uint8_t mod_len, uint32_t *p_pr_key, uint8_t pr_key_len)
{
        uint8_t i;

       rsa_data_TypeDef *de_pr_key,*de_n0b;
       rsa_data_TypeDef *de_mod;

       /*get pointer  Global variable */
       de_mod = __RSA_ENCRY_S.de_mod;
       de_pr_key = __RSA_ENCRY_S.de_pr_key;
       de_n0b = __RSA_ENCRY_S.de_n0b;

        /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < de_mod->len; i++)
                de_mod->addr[i] = 0x0;

        /* get new value*/
        de_mod->len = mod_len;
        for( i = 0; i < mod_len; i++)
                de_mod->addr[i] = p_mod[i];

         /* clr pubic key len,prevent program run in and reinitial */
        for( i = 0; i < de_pr_key->len; i++)
                de_pr_key->addr[i] = 0x0;

        /* get new value*/
        de_pr_key->len = pr_key_len;
        for( i = 0; i < pr_key_len; i++)
                de_pr_key->addr[i] = p_pr_key[i];

        /* get n0' data */
        de_n0b->addr[0] = de_mod->addr[0];
        __rsa_n0_inver(de_n0b->addr);

        return 0;

}

#endif

static inline void __rsa_mon_exp_f(rsa_data_TypeDef *x,rsa_data_TypeDef *a, rsa_data_TypeDef *pb_key)
{

        uint8_t test_shift_b,test_cnt;
        uint8_t i,j;
        uint32_t test_key;

        test_cnt = pb_key->len - 1;
        test_key = pb_key->addr[test_cnt]; /* pub_key high byte content */

        i = 0;
        while(!((test_key << i)& 0x80000000)){
                i ++;
        }
        test_key = test_key << i;
        test_shift_b = 32 - i;

        for(i = 0; i < test_shift_b; i++){
                 __rsa_mon_pro_double(x);
                if((test_key << i)& 0x80000000){
                        __rsa_mon_pro(x,a);
                }
        }
           /* others word handle */

        for(j = 0; j < test_cnt; j++){
                test_key = pb_key->addr[test_cnt -1 -j]; /* last 2 second word */

                for(i = 0; i < 32; i++){
                 __rsa_mon_pro_double(x);
                if((test_key << i)& 0x80000000){
                        __rsa_mon_pro(x,a);
                }
            }
        }
        __rsa_mon_pro_single(x);
}

void __rsa_mon_exp(rsa_data_TypeDef * mes, rsa_data_TypeDef *pb_key, rsa_data_TypeDef *mod)
{
        rsa_data_TypeDef *temp_x;
        uint8_t i;

       /*get pointer  Global variable */
       temp_x = __RSA_ENCRY_S.temp_x;

        /*shift mes RSA words width */
        for(i= 0; i < mes->len; i++){
                mes->addr[i + __RSA_WORDS_W] =  mes->addr[i];
                mes->addr[i] = 0x0;/*clear self */
        }
        mes->len += __RSA_WORDS_W;
        rsa_divide(mes->addr,&mes->len,mod->addr,&mod->len);/* get \a*/

        /* set r value */
          temp_x->len = __RSA_MAX_TEMP_X_DATA_CNT;
          __rsa_clr(temp_x);

          temp_x->len = __RSA_WORDS_W + 1;
          temp_x->addr[__RSA_WORDS_W] = 0x1;
          rsa_divide(temp_x->addr,&temp_x->len,mod->addr,&mod->len);/* get \x*/

          __rsa_mon_exp_f(temp_x,mes,pb_key);


}

uint8_t rsa_encrypt(uint32_t *p_mes, uint8_t mes_len)
{
#ifdef   __RSA_CONFIG_DECRYPT
  uint8_t i;

       rsa_data_TypeDef *mes ,*pb_key, *temp_x,*pb_n0b,*n0b;
       rsa_data_TypeDef *mod,*pb_mod;

       /*get pointer  Global variable */
       mod = __RSA_ENCRY_S.mod;
       mes = __RSA_ENCRY_S.mes;
       pb_key = __RSA_ENCRY_S.pb_key;
       pb_n0b = __RSA_ENCRY_S.pb_n0b;
       pb_mod = __RSA_ENCRY_S.pb_mod;
       temp_x =  __RSA_ENCRY_S.temp_x;
       n0b =  __RSA_ENCRY_S.n0b;

       /* get n'0 b */
       n0b->addr[0] = pb_n0b->addr[0];

        /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < mes->len; i++)
                mes->addr[i] = 0x0;

        /* get new value*/
        mes->len = mes_len;
        for( i = 0; i < mes->len; i++)
                mes->addr[i] = p_mes[i];

         /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < mod->len; i++)
                mod->addr[i] = 0x0;

        /* get new value*/
        mod->len = pb_mod->len;
        for( i = 0; i < mod->len; i++)
                mod->addr[i] = pb_mod->addr[i];

         __rsa_mon_exp(mes, pb_key,mod); /* mes ^ pb_key  % mod */

          /* return modify value ,here will check perior Zero*/
         for(i = 0; i < 32; i++)
               p_mes[i] = temp_x->addr[i];
#else
        uint8_t i;

       rsa_data_TypeDef *mes ,*pb_key, *temp_x;
       rsa_data_TypeDef *mod;

       /*get pointer  Global variable */
       mod = __RSA_ENCRY_S.mod;
       mes = __RSA_ENCRY_S.mes;
       pb_key = __RSA_ENCRY_S.pb_key;
       temp_x =  __RSA_ENCRY_S.temp_x;

        /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < mes->len; i++)
                mes->addr[i] = 0x0;

        /* get new value*/
        mes->len = mes_len;
        for( i = 0; i < mes->len; i++)
                mes->addr[i] = p_mes[i];

         __rsa_mon_exp(mes, pb_key,mod); /* mes ^ pb_key  % mod */

          /* return modify value ,here will check perior Zero*/
         for(i = 0; i < 32; i++)
               p_mes[i] = temp_x->addr[i];
#endif
         return 0;

}

#ifdef     __RSA_CONFIG_DECRYPT
uint8_t rsa_decrypt(uint32_t *p_mes, uint8_t mes_len)
{
        uint8_t i;

       rsa_data_TypeDef *mes ,*de_pr_key,*temp_x,*n0b,*de_n0b;
       rsa_data_TypeDef *mod,*de_mod;

       /*get pointer  Global variable */
        mes = __RSA_ENCRY_S.mes;
        mod = __RSA_ENCRY_S.mod;
        de_pr_key = __RSA_ENCRY_S.de_pr_key;
        temp_x =  __RSA_ENCRY_S.temp_x;
        de_n0b =  __RSA_ENCRY_S.de_n0b;
        n0b =  __RSA_ENCRY_S.n0b;
        de_mod =  __RSA_ENCRY_S.de_mod;

        n0b->addr[0] = de_n0b->addr[0];
        /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < mes->len; i++)
                mes->addr[i] = 0x0;

        /* get new value*/
        mes->len = mes_len;
        for( i = 0; i < mes->len; i++)
                mes->addr[i] = p_mes[i];

     /* clr mod len,prevent program run in and reinitial */
        for( i = 0; i < mod->len; i++)
                mod->addr[i] = 0x0;

        /* get new value*/
        mod->len = de_mod->len;
        for( i = 0; i < mod->len; i++)
                mod->addr[i] = de_mod->addr[i];

         __rsa_mon_exp(mes, de_pr_key, mod); /* mes ^ pb_key  % mod */

          /* return modify value ,here will check perior Zero*/
         for(i = 0; i < 32; i++)
               p_mes[i] = temp_x->addr[i];


         return 0;

}
#endif
/* need temp_des and temp_u varaible */

static inline void __rsa_mon_pro(rsa_data_TypeDef *sour_x, rsa_data_TypeDef *sour_y)
{

        uint32_t temp_c,temp_32m;
        uint64_t temp_q,temp_64m;

        int8_t temp_b;
        uint8_t i,j,k;
        rsa_data_TypeDef *temp_des ,*temp_u;
        rsa_data_TypeDef *mod,*n0b;

       /*get pointer  Global variable */
       temp_des = __RSA_ENCRY_S.temp_des;
       temp_u =  __RSA_ENCRY_S.temp_u;
       n0b =  __RSA_ENCRY_S.n0b;
       mod =  __RSA_ENCRY_S.mod;

        __rsa_memset(temp_des->addr, 0,2*RSA_SOS_BITS_S + 1);
        for( i = 0; i <= RSA_SOS_BITS_S -1; i++){
                temp_c = 0;
                for(j = 0; j <= RSA_SOS_BITS_S - 1; j++){
                        temp_q = (uint64_t)sour_x->addr[j] * sour_y->addr[i];
                        temp_q += temp_des->addr[i + j];
                        temp_q += temp_c;
                        temp_des->addr[i + j] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);
                }
                temp_des->addr[i + RSA_SOS_BITS_S] = temp_c;
        }

        for( i = 0; i <= RSA_SOS_BITS_S -1; i++){
                temp_c = 0;
                temp_64m = (uint64_t)temp_des->addr[i] * n0b->addr[0];
                temp_32m =(uint32_t)temp_64m;
                for(j = 0; j <= RSA_SOS_BITS_S - 1; j++){
                        temp_q = (uint64_t)mod->addr[j] * temp_32m;
                        temp_q += temp_des->addr[i + j];
                        temp_q += temp_c;
                        temp_des->addr[i + j] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);
                }

                 for(k = 0; k < (__RSA_MON_PRO_MAX_CARRY - RSA_SOS_BITS_S -i); k++){

                        temp_q = (uint64_t)temp_des->addr[i + RSA_SOS_BITS_S + k] + temp_c;
                        temp_des->addr[i + RSA_SOS_BITS_S + k] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);

                        if(temp_c ==0){
                                 break;
                        }
                 }
        }

        temp_u->addr[32] = 0x0;
        for( j = 0; j <= RSA_SOS_BITS_S; j++)
                temp_u->addr[j] =  temp_des->addr[j + RSA_SOS_BITS_S];
        temp_b = 0;
        for( i = 0 ; i <= RSA_SOS_BITS_S -1; i++){
                 temp_q = (int64_t)temp_u->addr[i] - mod->addr[i] + temp_b ; //k is -1 or 0
                 temp_des->addr[i] = (uint32_t)temp_q;
                 temp_b = (int8_t)(temp_q >> 32); /* get remainder */
        }

        temp_q = (int64_t)temp_u->addr[RSA_SOS_BITS_S] + temp_b;
        temp_des->addr[RSA_SOS_BITS_S] = (uint32_t)temp_q;
        temp_b = (int8_t)(temp_q >> RSA_SOS_BITS_S); /* get remainder */

        if(temp_b ==0){
                for(i = 0 ; i <= RSA_SOS_BITS_S -1 ;i++)
                sour_x->addr[i] = temp_des->addr[i];        //   return t[0].........t[s-1]
        }else{
                for(i = 0 ; i <= RSA_SOS_BITS_S -1 ;i++)
                sour_x->addr[i] = temp_u->addr[i];        //   return t[0].........t[s-1]
        }
}



static inline void __rsa_mon_pro_single(rsa_data_TypeDef *sour_x)
{

        uint32_t temp_c,temp_32m;
        uint64_t temp_q,temp_64m;

        int8_t temp_b;
        uint8_t i,j,k;

        rsa_data_TypeDef *temp_des ,*temp_u;
        rsa_data_TypeDef *mod,*n0b;

        /*get pointer  Global variable */
        temp_des = __RSA_ENCRY_S.temp_des;
        temp_u =  __RSA_ENCRY_S.temp_u;
        n0b =  __RSA_ENCRY_S.n0b;
        mod =  __RSA_ENCRY_S.mod;

        __rsa_memset(temp_des->addr, 0,2*RSA_SOS_BITS_S + 1);

        for( j = 0; j <= RSA_SOS_BITS_S; j++)
                temp_des->addr[j] =  sour_x->addr[j];

        for( i = 0; i <= RSA_SOS_BITS_S -1; i++){
                temp_c = 0;
                temp_64m = (uint64_t)temp_des->addr[i] * n0b->addr[0];
                temp_32m =(uint32_t)temp_64m;
                for(j = 0; j <= RSA_SOS_BITS_S - 1; j++){
                        temp_q = (uint64_t)mod->addr[j] * temp_32m;
                        temp_q += temp_des->addr[i + j];
                        temp_q += temp_c;
                        temp_des->addr[i + j] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);
                }

                 for(k = 0; k < (__RSA_MON_PRO_SINGLE_MAX_CARRY - RSA_SOS_BITS_S - i); k++){

                        temp_q = (uint64_t)temp_des->addr[i + RSA_SOS_BITS_S + k] + temp_c;
                        temp_des->addr[i + RSA_SOS_BITS_S + k] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);

                        if(temp_c ==0){
                                 break;
                        }

                 }
        }

        temp_u->addr[32] = 0x0;
        for( j = 0; j <= RSA_SOS_BITS_S; j++)
                temp_u->addr[j] =  temp_des->addr[j + RSA_SOS_BITS_S];
        temp_b = 0;
        for( i = 0 ; i <= RSA_SOS_BITS_S -1; i++){
                 temp_q = (int64_t)temp_u->addr[i] - mod->addr[i] + temp_b ; //k is -1 or 0
                 temp_des->addr[i] = (uint32_t)temp_q;
                 temp_b = (int8_t)(temp_q >> 32); /* get remainder */
        }

        temp_q = (int64_t)temp_u->addr[RSA_SOS_BITS_S] + temp_b;
        temp_des->addr[RSA_SOS_BITS_S] = (uint32_t)temp_q;
        temp_b = (int8_t)(temp_q >> RSA_SOS_BITS_S); /* get remainder */

        if(temp_b ==0){
                for(i = 0 ; i <= RSA_SOS_BITS_S -1 ;i++)
                sour_x->addr[i] = temp_des->addr[i];        //   return t[0].........t[s-1]
        }else{
                for(i = 0 ; i <= RSA_SOS_BITS_S -1 ;i++)
                sour_x->addr[i] = temp_u->addr[i];        //   return t[0].........t[s-1]
        }
}



/* need temp_des and temp_u varaible */

static inline void __rsa_mon_pro_double(rsa_data_TypeDef *sour_x)
{
        uint32_t temp_c,temp_32m;
        uint64_t temp_q,temp_64m;

        int8_t temp_b;
        uint8_t i,j,k;
        rsa_data_TypeDef *temp_des ,*temp_u;
        rsa_data_TypeDef *mod,*n0b;

       /*get pointer  Global variable */
       temp_des = __RSA_ENCRY_S.temp_des;
       temp_u =  __RSA_ENCRY_S.temp_u;
       n0b =  __RSA_ENCRY_S.n0b;
       mod =  __RSA_ENCRY_S.mod;

        __rsa_memset(temp_des->addr, 0,2*RSA_SOS_BITS_S + 1);

        for( i = 0; i <= RSA_SOS_BITS_S -2; i++){
                temp_c = 0;

                for(j = i + 1; j <= RSA_SOS_BITS_S - 1; j++){
                        temp_q = (uint64_t)sour_x->addr[j] * sour_x->addr[i];
                        temp_q += temp_des->addr[i + j];
                        temp_q += temp_c;
                        temp_des->addr[i + j] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);
                }
                 temp_des->addr[i + RSA_SOS_BITS_S] = temp_c;
        }
        k = 0;
        for(i = 0; i < RSA_SOS_BITS_S*2; i++){
          j =  (uint8_t)(temp_des->addr[i] >>31);  // save high bits
         /* bug fixed ""<<"" priority <<<<< "+"*/
          temp_des->addr[i] = (temp_des->addr[i] << 1) + (uint32_t)k;
          k = j;  /* inverse bits */
        }
        temp_c = 0;
        for( i = 0; i <= RSA_SOS_BITS_S -1; i++){
                        temp_q = (uint64_t)sour_x->addr[i] * sour_x->addr[i];
                        temp_q += temp_des->addr[i + i];
                        temp_q += temp_c;
                        temp_c = (uint32_t)(temp_q >> 32);
                        temp_des->addr[i + i ] = (uint32_t)temp_q;

                        for(k = 0; k < (__RSA_MON_PRO_DOU_MAX_CARRY + 1 - 2*i); k++){
                                temp_q = (uint64_t)temp_des->addr[i + i + 1 + k] + temp_c;
                                temp_des->addr[i + i + 1 + k] = (uint32_t)temp_q;
                                temp_c = (uint32_t)(temp_q >> 32);

                                if(temp_c ==0){
                                         break;
                                }
                         }
                }
     //   temp_des->addr[2*RSA_SOS_BITS_S -1 ] += temp_c;

        for( i = 0; i <= RSA_SOS_BITS_S -1; i++){
                temp_c = 0;
                temp_64m = (uint64_t)temp_des->addr[i] * n0b->addr[0];
                temp_32m =(uint32_t)temp_64m;
                for(j = 0; j <= RSA_SOS_BITS_S - 1; j++){
                        temp_q = (uint64_t)mod->addr[j] * temp_32m;
                        temp_q += temp_des->addr[i + j];
                        temp_q += temp_c;
                        temp_des->addr[i + j] = (uint32_t)temp_q;
                        temp_c = (uint32_t)(temp_q >> 32);
                }
                         for(k = 0; k < RSA_SOS_BITS_S; k++){

                                temp_q = (uint64_t)temp_des->addr[i + RSA_SOS_BITS_S + k] + temp_c;
                                temp_des->addr[i + RSA_SOS_BITS_S + k] = (uint32_t)temp_q;
                                temp_c = (uint32_t)(temp_q >> 32);

                                if(temp_c ==0){
                                         break;
                                }
                         }
        }

        temp_u->addr[32] = 0x0;
        for( j = 0; j <= RSA_SOS_BITS_S; j++)
                temp_u->addr[j] =  temp_des->addr[j + RSA_SOS_BITS_S];
        temp_b = 0;
        for( i = 0 ; i <= RSA_SOS_BITS_S -1; i++){
                 temp_q = (int64_t)temp_u->addr[i] - mod->addr[i] + temp_b ; //k is -1 or 0
                 temp_des->addr[i] = (uint32_t)temp_q;
                 temp_b = (int8_t)(temp_q >> 32); /* get remainder */
        }

        temp_q = (int64_t)temp_u->addr[RSA_SOS_BITS_S] + temp_b;
        temp_des->addr[RSA_SOS_BITS_S] = (uint32_t)temp_q;
        temp_b = (int8_t)(temp_q >> RSA_SOS_BITS_S); /* get remainder */

        if(temp_b ==0){
                for(i = 0 ; i <= RSA_SOS_BITS_S -1 ;i++)
                sour_x->addr[i] = temp_des->addr[i];        //   return t[0].........t[s-1]
        }else{
                for(i = 0 ; i <= RSA_SOS_BITS_S -1 ;i++)
                sour_x->addr[i] = temp_u->addr[i];        //   return t[0].........t[s-1]
        }
}


//#define RSA_TEST_MODE_CNT      32
//#define RSA_TEST_MODE_DATA    {\
//0X67598CC9,  0XAA3FD278,  0X049B64E7,  0X6B04EAB2,  0XACA78B9C,  0XF760B29E,  0XE432C8F9,  0X0FE17786,\
//0X0AD347E2,  0X3C328E63,  0X6F23CFAD,  0X48F74EB2,  0X1B17AA71,  0X0FE69715,  0XC30A8BFD,  0X88AD5DB3,\
//0X7EBEF3B5,  0X9171CDE2,  0X3ECB2B2D,  0X59596D2C,  0XF0235EF4,  0X0C107B62,  0XFC15D796,  0X868903DB,\
//0XC42016EA,  0X41784605,  0X949E0147,  0XB7FC8662,  0X05658A3B,  0X4F474D13,  0X281A21C3,  0X9D76CAC2}
//
//#define RSA_TEST_PB_KEY_CNT 1
//#define RSA_TEST_PB_KEY_DATA {0x10001}
//
//#define RSA_TEST_PR_KEY_CNT 32
//#define RSA_TEST_PR_KEY_DATA {\
//0X30409009,  0XBD6F7B7D,  0X2D0E6654,  0X1F5EEBCA,  0X03CDB701,  0X4B194C4C,  0X3B04D5BB,  0X226F467E,\
//0XBE4CF497,  0XB7969F50,  0X4AC26ED8,  0XE055F3B5,  0XC2847600,  0XD48D6B4E,  0X39E6366E,  0X09D1D6F3,\
//0XF08AEF82,  0XC20E7730,  0XDDE0C548,  0XC438886E,  0XD340F89C,  0X51676C2A,  0X86E9B6AD,  0XD025A5A8,\
//0X03D4B7EA,  0XCAD22E79,  0XFBF687E2,  0X9ACA7B8F,  0X396D1C79,  0XD8CEDDBF,  0X5C949FB4,  0X6A7DC64E}
//
//#define RSA_TEST_MES_CNT 32
//#define RSA_TEST_MES_DATA {0x12345678,0xffffffff,0xffffaa55}
//
//#pragma data_alignment = 4
//const char test_char1[33*4] = "Hello World! This is Gecko!";
//const char test_char2[33*4] = "Hello World! This is abandada!";
//const char test_char3[33*4] = "Helldsadsais is Geckdasdsado!";
//const char test_char4[33*4] = "Hello Worldsadasdsadasdascko!";
//
//uint32_t test_mod[RSA_TEST_MODE_CNT] = RSA_TEST_MODE_DATA;
//uint8_t  test_mod_len =   RSA_TEST_MODE_CNT;
//
//uint32_t test_pb_key[RSA_TEST_PB_KEY_CNT] = RSA_TEST_PB_KEY_DATA;
//uint8_t test_pb_key_len =   RSA_TEST_PB_KEY_CNT;
//
//uint32_t test_pr_key[RSA_TEST_PR_KEY_CNT] = RSA_TEST_PR_KEY_DATA;
//uint8_t test_pr_key_len =   RSA_TEST_PR_KEY_CNT;
//
//
//uint32_t test_mes[RSA_TEST_MES_CNT] = RSA_TEST_MES_DATA ;
//uint8_t test_mes_len =   RSA_TEST_MES_CNT;
//
//extern volatile uint32_t rsa_test_cnt  ;
//void rsa_test(void)
//{
//        uint32_t *test_p;
//        uint8_t i,j;
//        uint32_t  test_time2;
//        __rsa_struc_init();
//
//
//
//
//
//        rsa_encrypt_init(test_mod,test_mod_len,test_pb_key,test_pb_key_len);
//        rsa_decrypt_init(test_mod,test_mod_len,test_pr_key,test_pr_key_len);
//        while(1){
//        for(j = 0; j < 4; j++){
//
//          switch(j){
//                  case 0:test_p = (uint32_t *)test_char1;break;
//                  case 1:test_p = (uint32_t *)test_char2;break;
//                  case 2:test_p = (uint32_t *)test_char3;break;
//                  case 3 : test_p = (uint32_t *)test_char4;break;
//                  default:break;
//          }
//
//         for(i = 0; i < 32; i++)
//                  test_mes[i] = test_p[i];
//
//       rsa_test_cnt = 0;
//       printf("----------------------------------------RSA_encrypt_start!!!\n\r");
//       printf("Encrypt Message ===> %s\n\r",(uint8_t *)test_mes);
//        rsa_encrypt(test_mes,test_mes_len);
////        printf("Encrypt data as follow\n\r");
////        printf("[%2d]=%8X ",0,test_mes[0]);
////        for(i= 1; i < 32; i++){
////          printf("[%2d]=%8X ",i,test_mes[i]);
////          if((i+1)%4){
////          }else{
////            printf("\r\n");
////          }
////        }
//        test_time2 =  rsa_test_cnt ;
//        printf("Const time =%6d Ms\n\r",test_time2/100);
//        printf("----------------------------------------RSA_encrypt_end!!!\n\r");
//
//        printf("******************************\n\r");
//        printf("----------------------------------------RSA_decrypt_start!!!\n\r");
//        rsa_test_cnt = 0;
//        rsa_decrypt(test_mes,test_mes_len);
//
//        test_time2 =  rsa_test_cnt ;
//        printf("Const time =%6d Ms\n\r",test_time2/100);
//
//        printf("Dncrypt Message ===> %s\n\r",(uint8_t *)test_mes);
//        printf("----------------------------------------RSA_decrypt_end!!!\n\r");
//        printf("------------------------------------------------------------\n\r");
//        printf("------------------------------------------------------------\n\r");
//
//        }
//        j = 0;
//        }
//}



/*  des = des + sour
*  if des overfolow the return 1 otherwise 0
*  Note des  sour BUF must > acturelly +1 for Pointer overflow
*/


static uint8_t rsa_compare(uint32_t *des, uint32_t *sour, uint8_t length)
{
        if(length){
                do{
                        length--;
                        if(des[length] < sour[length]){
                                return 0;
                        }
                        else if(des[length] > sour[length]){
                                return 2;
                        }
                }while(length);
        }
        return 1;

}


static uint8_t rsa_add(uint32_t *des, uint8_t *des_len, uint32_t *sour, uint8_t *sour_len)
{
        uint32_t *des_low, *sour_low;
        uint8_t i;
        uint8_t k;
        uint64_t temp_q;
        int8_t dif_len;

        dif_len = *des_len - *sour_len;
        k = 0; /* intial remainder  */

        for( i = 0 ; i < *sour_len; i++){
                des_low = des + i;
                sour_low = sour + i;
                temp_q = (uint64_t)*des_low + *sour_low + k ;
                *des_low = (uint32_t)temp_q;

                k = (uint8_t)(temp_q >> RSA_CONSTANT_SHIFT); /* get remainder */

        }

        if(dif_len > 0){
                for(i = 0 ; i < dif_len; i++ ){
                        des_low = des + *sour_len + i;
                        temp_q = (uint64_t)*des_low + k ;
                        *des_low = (uint32_t)temp_q;
                        k = (uint8_t)(temp_q >> RSA_CONSTANT_SHIFT); /* get remainder */
                        if(k ==0){
                                break;
                        }
                }
        }else{
                *des_len = *sour_len ;
        }

        if(k ==1){
            //    *(des + i) = 1;
            //    *des_len = length + 1;
                return 1;
        }else{
                return 0;
        }
}

static int8_t rsa_sub(uint32_t *des, uint8_t *des_len, uint32_t *sour, uint8_t *sour_len)
{
        uint32_t *des_low, *sour_low;
        uint8_t i;
        int8_t k;
        uint64_t temp_q;
        int8_t dif_len;

        dif_len = *des_len - *sour_len;
        k = 0; /* intial remainder  */

        for( i = 0 ; i <  *sour_len; i++){
                des_low = des + i;
                sour_low = sour + i;

                temp_q = (int64_t)*des_low - *sour_low + k ; //k is -1 or 0
                *des_low = (uint32_t)temp_q;

                k = (int8_t)(temp_q >> RSA_CONSTANT_SHIFT); /* get remainder */
        }

        if(dif_len > 0){
                for(i = 0 ; i < dif_len; i++ ){
                        des_low = des + *sour_len + i;
                         temp_q = (int64_t)*des_low + k ; //k is -1 or 0
                         *des_low = (uint32_t)temp_q;

                        k = (int8_t)(temp_q >> RSA_CONSTANT_SHIFT); /* get remainder */
                        if(k ==0){
                                break;
                        }
                }
        }else{
                *des_len = *sour_len ;
        }

        return ((k ==-1) ? -1: 0);

}


/*return buf conut des_len + sour_len */
static void rsa_mul(uint32_t *des, uint8_t *des_len, uint32_t *sour, uint8_t *sour_len)
{
        uint8_t i,j;
        uint32_t *des_low, *sour_low;
        uint32_t *ret, *ret_buf;
        uint64_t t;
        uint32_t k;

        des_low = des;
        sour_low = sour;
        ret = ret_buf = __RSA_MUL_BUF;
        /* clear public buf */
        __rsa_memset(ret, 0,*des_len + *sour_len);

        k = 0;
        t = 0;
         for(i = 0; i < *des_len; i++){
           if(*des_low){
                   ret = ret_buf + i;
                   sour_low = sour;
                   k = 0;
                    for(j = 0; j < *sour_len; j++ ){
                        t= (uint64_t)(*des_low) * (*sour_low);
                        t +=  *ret ;/*don't over folow 64 bits */
                        t += k;
                        *ret = (uint32_t)t; //get remaider
                        k = (uint32_t)(t >> RSA_CONSTANT_SHIFT); //quotinet

                        ret ++;
                        sour_low ++;
                   }
                      *ret = k;
           }
           des_low ++;
        }
         *des_len = *des_len + *sour_len;
            if(ret_buf[*des_len - 1] ==0x0){
                  *des_len -= 1;
         }
        /*shift buf */
        __rsa_memset(des, 0,RSA_MAX_DES_BUF_CNT);

        for(i = 0; i < *des_len; i ++)
              des[i] = ret_buf[i];
}

/*  des = des / sour */
/* Note  soure_len 32 bit */
 inline	static void rsa_divide(uint32_t *des, uint8_t *des_len, uint32_t *sour, uint8_t *sour_len)
	{
       uint32_t *des_high, *sour_high;
        uint8_t i, j;
        uint8_t length;
        uint32_t  *d_temp_sour;
        uint32_t d_temp_q;
        uint64_t d_temp_a,d_q;
        uint64_t dd1,dd2;
        uint8_t d_temp_sour_len1, d_temp_sour_len2;

  if(*des_len <= *sour_len){
    while(!rsa_compare(des, sour, *sour_len)){
      return;
    }
  }

  length = *des_len - *sour_len + 1;
  sour_high = sour + *sour_len - 1; // will modfiy BUGBUG

  for( i = 0 ; i < length ; i++){
        des_high  = des + *des_len  -i;  // des_high  = des + *des_len -1 - i;

        d_temp_a =  ((uint64_t)(*des_high)) << 32;
        d_temp_a += *(des_high - 1);

        if(rsa_compare(des_high, sour_high, 1) == 1){
                d_q = 0xFFFFFFFF;
        }else{
                d_q = d_temp_a / (*sour_high); //(Uj * B + Uj+1)/V1 maxbit 32bit for *sour
        }
        while(1){
                dd1= d_q * *(sour_high - 1);
                dd2 = d_q * (*sour_high);
                dd2 = d_temp_a - dd2;
                if(dd2 >  0xFFFFFFFF){
                        break;
                }
                dd2 = dd2 << 32;
                if(( *des_len - i) < 2){

                }else{

                        dd2 = dd2 + *(des_high - 2);
                }
                if(dd1 > dd2){
                        d_q--;
                }else{
                        break;
                }
        }

        d_temp_q = (uint32_t)d_q;

        d_temp_sour = __RSA_DIVIDE_BUF;
        /* clear public buf */
        d_temp_sour_len2 = 1;
        __rsa_memset(d_temp_sour, 0, *sour_len + d_temp_sour_len2);

        for(j = 0; j < *sour_len; j++ )
                d_temp_sour[j] = sour[j];
        d_temp_sour_len1 = *sour_len;

        rsa_mul(d_temp_sour, &d_temp_sour_len1, &d_temp_q, &d_temp_sour_len2); /* mul result will add 32 bits */

        d_temp_sour_len2 = *sour_len + 1; /* set sub enqual */
        /* sub method use high positon adjust des pointer */
        if(-1 ==(rsa_sub((des_high  - d_temp_sour_len2 + 1) , &d_temp_sour_len2, d_temp_sour, &d_temp_sour_len1))){/* check over carry */
                d_q --;

//                for( k = 0 ; k < d_temp_sour_len2 ; k++){
//                      d_temp_q = *(des_high  - d_temp_sour_len2 + 1 + k);
//                      *(des_high  - d_temp_sour_len2 + 1 + k) = ~d_temp_q;
//                }
//                rsa_add_carry((des_high - d_temp_sour_len2 + 1), d_temp_sour_len2, 1);
                rsa_add((des_high - d_temp_sour_len2 + 1), &d_temp_sour_len2, sour, sour_len);
        }
       // (quot->addr)[quot->len ++] = d_q;
        // q is quotient
  }
        /* get remaider, now the Des pointer content is */
        *des_len = *sour_len;
        /*clear buf */
     //   rsa_memset(des->addr + sour->len, 0,RSA_INVER_MAX_CNT - sour->len);

            /* get real return value */
        for(i = 0 ; i < *des_len; i++){
                if(*(des + *des_len - 1 - i) != 0x0){
                        break;
                }
        }
        if(*des_len != 0x1){
                *des_len -= i;
        }
}

ByteArray RSA::Encrypt(const Array& data, const Array& pass)
{
	/*byte buf[KeyLength];
	ByteArray box(buf, KeyLength);

	byte block1[256];
	//byte block2[256];
	byte tempbuf[256];
	powTbl = block1;
	logTbl = tempbuf;
	CalcPowLog( powTbl, logTbl );

	sBox = block2;
	CalcSBox( sBox );

	expandedKey = block1;
	KeyExpansion( expandedKey );*/

	ByteArray rs;
	//rs.Copy(data);

	// 加密
	//aesEncrypt(rs.GetBuffer(), box.GetBuffer());

	return rs;
}

ByteArray RSA::Decrypt(const Array& data, const Array& pass)
{
	/*byte buf[KeyLength];
	ByteArray box(buf, KeyLength);

	byte block1[256];
	//byte block2[256];
	byte tempbuf[256];
	powTbl = block1;
	logTbl = block2;
	CalcPowLog( powTbl, logTbl );

	sBox = tempbuf;
	CalcSBox( sBox );

	expandedKey = block1;
	KeyExpansion( expandedKey );

	sBoxInv = block2; // Must be block2.
	CalcSBoxInv( sBox, sBoxInv );*/

	ByteArray rs;
	//rs.Copy(data);

	// 解密
	//aesDecrypt(rs.GetBuffer(), box.GetBuffer());

	return rs;
}
