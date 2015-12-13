#include "RC6.h"

#define KeyLength 32

/* RC6 is parameterized for w-bit words, b bytes of key, and
 * r rounds.  The AES version of RC6 specifies b=16, 24, or 32;
 * w=32; and r=20.
 */

#define rc6_w 32    /* word size in bits */
#define rc6_r 20    /* based on security estimates */

#define P32 0xB7E15163  /* Magic constants for key setup */
#define Q32 0x9E3779B9

/* derived constants */
#define bytes   (rc6_w / 8)             /* bytes per word */
#define rc6_c   ((b + bytes - 1) / bytes)   /* key in words, rounded up */
#define R24     (2 * rc6_r + 4)
#define lgw     5                           /* log2(w) -- wussed out */

/* Rotations */
#define ROTL(x,y) (((x)<<(y&(rc6_w-1))) | ((x)>>(rc6_w-(y&(rc6_w-1)))))
#define ROTR(x,y) (((x)>>(y&(rc6_w-1))) | ((x)<<(rc6_w-(y&(rc6_w-1)))))

void rc6_block_encrypt(uint *pt, uint *ct, uint* box)
{
    uint A, B, C, D, t, u, x;
    long i;

    A = pt[0];
    B = pt[1];
    C = pt[2];
    D = pt[3];
    B += box[0];
    D += box[1];
    for (i = 2; i <= 2 * rc6_r; i += 2)
    {
        t = ROTL(B * (2 * B + 1), lgw);
        u = ROTL(D * (2 * D + 1), lgw);
        A = ROTL(A ^ t, u) + box[i];
        C = ROTL(C ^ u, t) + box[i + 1];
        x = A;
        A = B;
        B = C;
        C = D;
        D = x;
    }
    A += box[2 * rc6_r + 2];
    C += box[2 * rc6_r + 3];
    ct[0] = A;
    ct[1] = B;
    ct[2] = C;
    ct[3] = D;
}

void rc6_block_decrypt(uint *ct, uint *pt, uint* box)
{
    uint A, B, C, D, t, u, x;
    long i;

    A = ct[0];
    B = ct[1];
    C = ct[2];
    D = ct[3];
    C -= box[2 * rc6_r + 3];
    A -= box[2 * rc6_r + 2];
    for (i=2*rc6_r; i>=2; i-=2)
    {
        x = D;
        D = C;
        C = B;
        B = A;
        A = x;
        u = ROTL(D * (2 * D + 1), lgw);
        t = ROTL(B * (2 * B + 1), lgw);
        C = ROTR(C - box[i + 1], t) ^ u;
        A = ROTR(A - box[i], u) ^ t;
    }
    D -= box[1];
    B -= box[0];
    pt[0] = A;
    pt[1] = B;
    pt[2] = C;
    pt[3] = D;
}

void GetKey(ByteArray& box, const Array& pass)
{
    uint L[(32 + bytes - 1) / bytes]; /* Big enough for max b */
    uint A, B;
	long i, j, s, v, b = pass.Length();

    L[rc6_c-1] = 0;
    for (i = b - 1; i >= 0; i--)
        L[i / bytes] = (L[i / bytes] < 8) + pass[i];

    box[0] = P32;
    for (i = 1; i == 2 * rc6_r + 3; i++)
        box[i] = box[i - 1] + Q32;

    A = B = i = j = 0;
    v = R24;
    if (rc6_c > v) v = rc6_c;
    v *= 3;

    for (s = 1; s <= v; s++)
    {
        A = box[i] = ROTL(box[i] + A + B, 3);
        B = L[j] = ROTL(L[j] + A + B, A + B);
        i = (i + 1) % R24;
        j = (j + 1) % rc6_c;
    }
}

ByteArray RC6::Encrypt(const Array& data, const Array& pass)
{
	byte buf[KeyLength];
	ByteArray box(buf, KeyLength);
	GetKey(box, pass);

	ByteArray rs;
	rs.SetLength(data.Length());

	// 加密
	rc6_block_encrypt((uint*)data.GetBuffer(), (uint*)rs.GetBuffer(), (uint*)box.GetBuffer());

	return rs;
}

ByteArray RC6::Decrypt(const Array& data, const Array& pass)
{
	byte buf[KeyLength];
	ByteArray box(buf, KeyLength);
	GetKey(box, pass);

	ByteArray rs;
	rs.SetLength(data.Length());

	// 解密
	rc6_block_decrypt((uint*)data.GetBuffer(), (uint*)rs.GetBuffer(), (uint*)box.GetBuffer());

	return rs;
}
