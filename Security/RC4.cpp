#include "RC4.h"

#define KeyLength 256

void GetKey(byte* box, uint len, byte* pass, uint plen)
{
	for (int i = 0; i < len; i++)
	{
		box[i] = i;
	}
	int j = 0;
	for (int i = 0; i < len; i++)
	{
		j = (j + box[i] + pass[i % plen]) % len;
		byte temp = box[i];
		box[i] = box[j];
		box[j] = temp;
	}
}

void RC4::Encrypt(byte* data, uint len, byte* pass, uint plen)
{
	assert_ptr(data);
	assert_ptr(pass);

	int i = 0;
	int j = 0;
	byte box[KeyLength];
	GetKey(box, ArrayLength(box), pass, plen);
	// 加密  
	for (int k = 0; k < len; k++)
	{
		i = (i + 1) % KeyLength;
		j = (j + box[i]) % KeyLength;
		byte temp = box[i];
		box[i] = box[j];
		box[j] = temp;
		byte a = data[k];
		byte b = box[(box[i] + box[j]) % KeyLength];
		data[k] = (byte)(a ^ b);
	}
}
