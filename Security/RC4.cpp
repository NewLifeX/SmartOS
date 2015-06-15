#include "RC4.h"

void GetKey(byte* box, byte* pass, Int32 len)
{
	for (int i = 0; i < len; i++)
	{
		box[i] = i;
	}
	int j = 0;
	for (int i = 0; i < len; i++)
	{
		j = (j + box[i] + pass[i % pass.Length]) % len;
		var temp = box[i];
		box[i] = box[j];
		box[j] = temp;
	}
	return box;
}

void RC4::Encrypt(byte* data, uint len, byte* pass, uint plen)
{
	assert_ptr(data);
	assert_ptr(pass);

	int i = 0;
	int j = 0;
	byte box[256];
	GetKey(box, pass, ArrayLength(box));
	// 加密  
	for (int k = 0; k < data.Length; k++)
	{
		i = (i + 1) % box.Length;
		j = (j + box[i]) % box.Length;
		byte temp = box[i];
		box[i] = box[j];
		box[j] = temp;
		byte a = data[k];
		byte b = box[(box[i] + box[j]) % box.Length];
		data[k] = (byte)(a ^ b);
	}
}
