#include "RC4.h"

#define KeyLength 256

void GetKey(ByteArray& box, const Buffer& pass)
{
	for (int i = 0; i < box.Length(); i++)
	{
		box[i] = i;
	}
	int j = 0;
	for (int i = 0; i < box.Length(); i++)
	{
		j = (j + box[i] + pass[i % pass.Length()]) % box.Length();
		byte temp = box[i];
		box[i] = box[j];
		box[j] = temp;
	}
}

void RC4::Encrypt(Buffer& data, const Buffer& pass)
{
	TS("RC4::Encrypt");

	int i = 0;
	int j = 0;
	byte buf[KeyLength];
	ByteArray box(buf, KeyLength);
	GetKey(box, pass);

	// 加密
	for (int k = 0; k < data.Length(); k++)
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

ByteArray RC4::Encrypt(const Buffer& data, const Buffer& pass)
{
	ByteArray rs;
	//rs.SetLength(data.Length());
	//rs	= data;
	rs.Copy(0, data, 0, data.Length());

	Encrypt(rs, pass);

	return rs;
}
