#include "String.h"

char _ChNil = '\0';
int _InitData[] = { -1, 0, 0, 0 }; //初始化CStringData的地址
CStringData* _DataNil = (CStringData*)&_InitData; //地址转化为CStringData*
char* _PchNil = (char*)(((byte*)&_InitData)+sizeof(CStringData));

//建立一个空的CString
const CString&  _GetEmptyString()
{
	return *(CString*)&_PchNil;
}

void CString::Init() { m_pchData=_GetEmptyString().m_pchData; }


CString::CString() { Init(); }

int CString::GetLength() const { return GetData()->nDataLength; }
int CString::GetAllocLength() const { return GetData()->nAllocLength; }
bool CString::IsEmpty() const { return GetData()->nDataLength == 0; }

CStringData* CString::GetData() const
{
	assert_param(m_pchData != NULL);
	return ((CStringData*)m_pchData)-1;
}

CString::operator char*() const { return m_pchData; }
int CString::SafeStrlen(char* lpsz) { return (lpsz == NULL) ? 0 : strlen(lpsz); }

void CString::AllocBuffer(int nLen)
{
	assert_param(nLen >= 0);
	assert_param(nLen <= 2147483647-1);    // (signed) int 的最大值

	if (nLen == 0)
		Init();
	else
	{
		CStringData* pData;
		{
			pData = (CStringData*) new byte[sizeof(CStringData) + (nLen+1)*sizeof(char)];
			pData->nAllocLength = nLen;
		}
		pData->nRefs = 1;
		pData->data()[nLen] = '\0';
		pData->nDataLength = nLen;
		m_pchData = pData->data();
	}
}

void CString::FreeData(CStringData* pData)
{
	delete[] (byte*)pData;
}

void CString::CopyBeforeWrite()
{
	if (GetData()->nRefs > 1)
	{
		CStringData* pData = GetData();
		Release();
		AllocBuffer(pData->nDataLength);
		memcpy(m_pchData, pData->data(), (pData->nDataLength+1)*sizeof(char));
	}
	assert_param(GetData()->nRefs <= 1);
}

void CString::AllocBeforeWrite(int nLen)
{
	if (GetData()->nRefs > 1 || nLen > GetData()->nAllocLength)
	{
		Release();
		AllocBuffer(nLen);
	}
	assert_param(GetData()->nRefs <= 1);
}

void CString::AssignCopy(int nSrcLen, char* lpszSrcData)
{
	AllocBeforeWrite(nSrcLen);
	memcpy(m_pchData, lpszSrcData, nSrcLen*sizeof(char));
	GetData()->nDataLength = nSrcLen;
	m_pchData[nSrcLen] = '\0';
}

void CString::AllocCopy(CString& dest, int nCopyLen, int nCopyIndex, int nExtraLen) const
{
	int nNewLen = nCopyLen + nExtraLen;
	if (nNewLen == 0)
	{
		dest.Init();
	}
	else
	{
		dest.AllocBuffer(nNewLen);
		memcpy(dest.m_pchData, m_pchData+nCopyIndex, nCopyLen*sizeof(char));
	}
}


CString::~CString()
{
	if (GetData() != _DataNil)
	{
		if (--(GetData()->nRefs) <= 0) FreeData(GetData());
	}
}


CString::CString(const CString& stringSrc)
{
	assert_param(stringSrc.GetData()->nRefs != 0);
	if (stringSrc.GetData()->nRefs >= 0)
	{
		assert_param(stringSrc.GetData() != _DataNil);
		m_pchData = stringSrc.m_pchData;
		GetData()->nRefs++;
	}
	else
	{
		Init();
		*this = stringSrc.m_pchData;
	}
}

CString::CString(char* lpsz)
{
	Init();
	int nLen = SafeStrlen(lpsz);
	if (nLen != 0)
	{
		AllocBuffer(nLen);
		memcpy(m_pchData, lpsz, nLen*sizeof(char));
	}
}

CString::CString(char* lpch, int nLength)
{
	Init();
	if (nLength != 0)
	{
		assert_param(lpch);
		AllocBuffer(nLength);
		memcpy(m_pchData, lpch, nLength*sizeof(char));
	}
}

void CString::Release()
{
	if (GetData() != _DataNil)
	{
		assert_param(GetData()->nRefs != 0);
		if (--(GetData()->nRefs) <= 0) FreeData(GetData());
		Init();
	}
}

void CString::Release(CStringData* pData)
{
	if (pData != _DataNil)
	{
		assert_param(pData->nRefs != 0);
		if (--(pData->nRefs) <= 0) FreeData(pData);
	}
}

void CString::Empty()
{
	if (GetData()->nDataLength == 0) return;
	if (GetData()->nRefs >= 0)
		Release();
	else
		*this = &_ChNil;
	assert_param(GetData()->nDataLength == 0);
	assert_param(GetData()->nRefs < 0 || GetData()->nAllocLength == 0);
}

const CString& CString::operator=(const CString& stringSrc)
{
	if (m_pchData != stringSrc.m_pchData)
	{
		if ((GetData()->nRefs < 0 && GetData() != _DataNil) || stringSrc.GetData()->nRefs < 0)
		{
			//新建一快数据
			AssignCopy(stringSrc.GetData()->nDataLength, stringSrc.m_pchData);
		}
		else
		{
			//只拷贝指针
			Release();
			assert_param(stringSrc.GetData() != _DataNil);
			m_pchData = stringSrc.m_pchData;
			GetData()->nRefs++;
		}
	}
	return *this;
}

const CString& CString::operator=(char* lpsz)
{
	assert_param(lpsz == NULL || lpsz);
	AssignCopy(SafeStrlen(lpsz), lpsz);
	return *this;
}

const CString& CString::operator=(char ch)
{
	AssignCopy(1, &ch);
	return *this;
}


int CString::Delete(int nIndex, int nCount /* = 1 */)
{
	if (nIndex < 0) nIndex = 0;
	int nNewLength = GetData()->nDataLength;
	if (nCount > 0 && nIndex < nNewLength)
	{
		CopyBeforeWrite(); //脱离共享数据块，
		int nBytesToCopy = nNewLength - (nIndex + nCount) + 1;
		//移动数据
		memcpy(m_pchData + nIndex,
		m_pchData + nIndex + nCount, nBytesToCopy * sizeof(char));
		GetData()->nDataLength = nNewLength - nCount;
	}
	return nNewLength;
}

int CString::Insert(int nIndex, char ch)
{
	CopyBeforeWrite(); //脱离共享数据

	if (nIndex < 0) nIndex = 0;

	int nNewLength = GetData()->nDataLength;
	if (nIndex > nNewLength) nIndex = nNewLength;
	nNewLength++;

	if (GetData()->nAllocLength < nNewLength)
	{
		//动态分配内存,并拷贝原来的数据
		CStringData* pOldData = GetData();
		char* pstr = m_pchData;
		AllocBuffer(nNewLength);
		memcpy(m_pchData, pstr, (pOldData->nDataLength+1)*sizeof(char));
		CString::Release(pOldData);
	}
	//插入数据
	memcpy(m_pchData + nIndex + 1,
	m_pchData + nIndex, (nNewLength-nIndex)*sizeof(char));
	m_pchData[nIndex] = ch;
	GetData()->nDataLength = nNewLength;

	return nNewLength;
}


int CString::Insert(int nIndex, char* pstr)
{
	if (nIndex < 0)
	nIndex = 0;

	int nInsertLength = SafeStrlen(pstr);
	int nNewLength = GetData()->nDataLength;
	if (nInsertLength > 0)
	{
		CopyBeforeWrite(); //脱离共享数据
		if (nIndex > nNewLength)
		nIndex = nNewLength;
		nNewLength += nInsertLength;

		if (GetData()->nAllocLength < nNewLength)
		{ //动态分配内存,并拷贝原来的数据
			CStringData* pOldData = GetData();
			char* pstr = m_pchData;
			AllocBuffer(nNewLength);
			memcpy(m_pchData, pstr, (pOldData->nDataLength+1)*sizeof(char));
			CString::Release(pOldData);
		}

		//移动数据，留出插入的位子move也可以
		memcpy(m_pchData + nIndex + nInsertLength,
		m_pchData + nIndex,
		(nNewLength-nIndex-nInsertLength+1)*sizeof(char));
		//插入数据
		memcpy(m_pchData + nIndex,
		pstr, nInsertLength*sizeof(char));
		GetData()->nDataLength = nNewLength;
	}

	return nNewLength;
}

int CString::Replace(char chOld, char chNew)
{
	int nCount = 0;
	if (chOld != chNew) //替换的不能相同
	{
		CopyBeforeWrite();
		char* psz = m_pchData;
		char* pszEnd = psz + GetData()->nDataLength;
		while (psz < pszEnd)
		{
			if (*psz == chOld) //替换
			{
				*psz = chNew;
				nCount++;
			}
			psz++; //相当于++psz,考虑要UNICODE下版本才用的
		}
	}
	return nCount;
}

int CString::Replace(char* lpszOld, char* lpszNew)
{
	int nSourceLen = SafeStrlen(lpszOld);
	if (nSourceLen == 0) //要替换的不能为空
	return 0;
	int nReplacementLen = SafeStrlen(lpszNew);

	int nCount = 0;
	char* lpszStart = m_pchData;
	char* lpszEnd = m_pchData + GetData()->nDataLength;
	char* lpszTarget;
	while (lpszStart < lpszEnd) //检索要替换的个数
	{
		while ((lpszTarget = strstr(lpszStart, lpszOld)) != NULL)
		{
			nCount++;
			lpszStart = lpszTarget + nSourceLen;
		}
		lpszStart += strlen(lpszStart) + 1;
	}


	if (nCount > 0)
	{
		CopyBeforeWrite();
		int nOldLength = GetData()->nDataLength;
		int nNewLength =  nOldLength + (nReplacementLen-nSourceLen)*nCount; //替换以后的长度
		if (GetData()->nAllocLength < nNewLength || GetData()->nRefs > 1)
		{
			//超出原来的内存长度动态分配
			CStringData* pOldData = GetData();
			char* pstr = m_pchData;
			AllocBuffer(nNewLength);
			memcpy(m_pchData, pstr, pOldData->nDataLength*sizeof(char));
			CString::Release(pOldData);
		}

		lpszStart = m_pchData;
		lpszEnd = m_pchData + GetData()->nDataLength;


		while (lpszStart < lpszEnd) //这个循环好象没什么用
		{
			while ( (lpszTarget = strstr(lpszStart, lpszOld)) != NULL) //开始替换
			{
				int nBalance = nOldLength - (lpszTarget - m_pchData + nSourceLen); //要往后移的长度
				//移动数据，留出插入的位子
				memmove(lpszTarget + nReplacementLen, lpszTarget + nSourceLen,
				nBalance * sizeof(char));
				//插入替换数据
				memcpy(lpszTarget, lpszNew, nReplacementLen*sizeof(char));
				lpszStart = lpszTarget + nReplacementLen;
				lpszStart[nBalance] = '\0';
				nOldLength += (nReplacementLen - nSourceLen); //现有数据长度
			}
			lpszStart += strlen(lpszStart) + 1;
		}
		assert_param(m_pchData[nNewLength] == '\0');
		GetData()->nDataLength = nNewLength;
	}

	return nCount;
}


int CString::Remove(char chRemove)
{
	CopyBeforeWrite();

	char* pstrSource = m_pchData;
	char* pstrDest = m_pchData;
	char* pstrEnd = m_pchData + GetData()->nDataLength;

	while (pstrSource < pstrEnd)
	{
		if (*pstrSource != chRemove)
		{
			*pstrDest = *pstrSource; //把不移除的数据拷贝
			pstrDest++;
		}
		pstrSource++;//++pstrSource
	}
	*pstrDest = '\0';
	int nCount = pstrSource - pstrDest; //比较变态的计算替换个数,
	GetData()->nDataLength -= nCount;

	return nCount;
}


CString CString::Mid(int nFirst) const
{
	return Mid(nFirst, GetData()->nDataLength - nFirst);
}

CString CString::Mid(int nFirst, int nCount) const
{
	if (nFirst < 0) nFirst = 0;
	if (nCount < 0) nCount = 0;

	if (nFirst + nCount > GetData()->nDataLength) nCount = GetData()->nDataLength - nFirst;
	if (nFirst > GetData()->nDataLength) nCount = 0;

	assert_param(nFirst >= 0);
	assert_param(nFirst + nCount <= GetData()->nDataLength);

	//取去整个数据
	if (nFirst == 0 && nFirst + nCount == GetData()->nDataLength) return *this;

	CString dest;
	AllocCopy(dest, nCount, nFirst, 0);
	return dest;
}


CString CString::Right(int nCount) const
{
	if (nCount < 0) nCount = 0;
	if (nCount >= GetData()->nDataLength) return *this;

	CString dest;
	AllocCopy(dest, nCount, GetData()->nDataLength-nCount, 0);
	return dest;
}

CString CString::Left(int nCount) const
{
	if (nCount < 0) nCount = 0;
	if (nCount >= GetData()->nDataLength) return *this;

	CString dest;
	AllocCopy(dest, nCount, 0, 0);
	return dest;
}


int CString::ReverseFind(char ch) const
{
	//从最后查找
	char* lpsz = strchr(m_pchData, ch);
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

int CString::Find(char ch) const
{
	return Find(ch, 0);
}

int CString::Find(char ch, int nStart) const
{
	int nLength = GetData()->nDataLength;
	if (nStart >= nLength) return -1;

	char* lpsz = strchr(m_pchData + nStart, ch);
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

int CString::Find(char* lpszSub) const
{
	return Find(lpszSub, 0);
}

int CString::Find(char* lpszSub, int nStart) const
{
	assert_param(lpszSub);

	int nLength = GetData()->nDataLength;
	if (nStart > nLength) return -1;

	char* lpsz = strstr(m_pchData + nStart, lpszSub);

	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

int CString::FindOneOf(char* lpszCharSet) const
{
	assert_param(lpszCharSet);
	char* lpsz = _tcspbrk(m_pchData, lpszCharSet);
	return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

void CString::MakeUpper()
{
	CopyBeforeWrite();
	_tcsupr(m_pchData);
}

void CString::MakeLower()
{
	CopyBeforeWrite();
	_tcslwr(m_pchData);
}

void CString::MakeReverse()
{
	CopyBeforeWrite();
	_tcsrev(m_pchData);
}

void CString::SetAt(int nIndex, char ch)
{
	assert_param(nIndex >= 0);
	assert_param(nIndex < GetData()->nDataLength);

	CopyBeforeWrite();
	m_pchData[nIndex] = ch;
}

void CString::TrimRight(char* lpszTargetList)
{
	CopyBeforeWrite();
	char* lpsz = m_pchData;
	char* lpszLast = NULL;

	while (*lpsz != '\0')
	{
		if (strchr(lpszTargetList, *lpsz) != NULL)
		{
			if (lpszLast == NULL) lpszLast = lpsz;
		}
		else
			lpszLast = NULL;
		lpsz++;
	}

	if (lpszLast != NULL)
	{
		*lpszLast = '\0';
		GetData()->nDataLength = lpszLast - m_pchData;
	}
}

void CString::TrimRight(char chTarget)
{
	CopyBeforeWrite();
	char* lpsz = m_pchData;
	char* lpszLast = NULL;

	while (*lpsz != '\0')
	{
		if (*lpsz == chTarget)
		{
			if (lpszLast == NULL) lpszLast = lpsz;
		}
		else
			lpszLast = NULL;
		lpsz++;
	}

	if (lpszLast != NULL)
	{
		*lpszLast = '\0';
		GetData()->nDataLength = lpszLast - m_pchData;
	}
}

void CString::TrimRight()
{
	CopyBeforeWrite();
	char* lpsz = m_pchData;
	char* lpszLast = NULL;

	while (*lpsz != '\0')
	{
		if (_istspace(*lpsz))
		{
			if (lpszLast == NULL) lpszLast = lpsz;
		}
		else
			lpszLast = NULL;
		lpsz++;
	}

	if (lpszLast != NULL)
	{
		// truncate at trailing space start
		*lpszLast = '\0';
		GetData()->nDataLength = lpszLast - m_pchData;
	}
}

void CString::TrimLeft(char* lpszTargets)
{
	// if we're not trimming anything, we're not doing any work
	if (SafeStrlen(lpszTargets) == 0) return;

	CopyBeforeWrite();
	char* lpsz = m_pchData;

	while (*lpsz != '\0')
	{
		if (strchr(lpszTargets, *lpsz) == NULL) break;
		lpsz++;
	}

	if (lpsz != m_pchData)
	{
		// fix up data and length
		int nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
		memmove(m_pchData, lpsz, (nDataLength+1)*sizeof(char));
		GetData()->nDataLength = nDataLength;
	}
}

void CString::TrimLeft(char chTarget)
{
	CopyBeforeWrite();
	char* lpsz = m_pchData;

	while (chTarget == *lpsz) lpsz++;

	if (lpsz != m_pchData)
	{
		int nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
		memmove(m_pchData, lpsz, (nDataLength+1)*sizeof(char));
		GetData()->nDataLength = nDataLength;
	}
}

void CString::TrimLeft()
{
	CopyBeforeWrite();
	char* lpsz = m_pchData;

	while (_istspace(*lpsz)) lpsz++;

	if (lpsz != m_pchData)
	{
		int nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
		memmove(m_pchData, lpsz, (nDataLength+1)*sizeof(char));
		GetData()->nDataLength = nDataLength;
	}
}

#define TCHAR_ARG   char
#define WCHAR_ARG   WCHAR
#define CHAR_ARG    char

struct _AFX_DOUBLE  { byte doubleBits[sizeof(double)]; };

#ifdef _X86_
#define DOUBLE_ARG  _AFX_DOUBLE
#else
#define DOUBLE_ARG  double
#endif

#define FORCE_ANSI      0x10000
#define FORCE_UNICODE   0x20000
#define FORCE_INT64     0x40000

void CString::FormatV(char* lpszFormat, va_list argList)
{
	assert_param(lpszFormat);

	va_list argListSave = argList;

	// make a guess at the maximum length of the resulting string
	int nMaxLen = 0;
	for (char* lpsz = lpszFormat; *lpsz != '\0'; lpsz++)
	{
		//查找%,对%%不在查找范围
		if (*lpsz != '%' || *(lpsz++) == '%')
		{
			nMaxLen += _tclen(lpsz);
			continue;
		}

		int nItemLen = 0;

		//%后面的格式判断
		int nWidth = 0;
		for (; *lpsz != '\0'; lpsz++)
		{
			if (*lpsz == '#')
				nMaxLen += 2;   // 16进制 '0x'
			else if (*lpsz == '*')
				nWidth = va_arg(argList, int);
			else if (*lpsz == '-' || *lpsz == '+' || *lpsz == '0' || *lpsz == ' ')
				;
			else // hit non-flag character
				break;
		}
		// get width and skip it
		if (nWidth == 0)
		{
			// width indicated by
			nWidth = _ttoi(lpsz);
			for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz++);
		}
		assert_param(nWidth >= 0);

		int nPrecision = 0;
		if (*lpsz == '.')
		{
			// skip past '.' separator (width.precision)
			lpsz++;

			// get precision and skip it
			if (*lpsz == '*')
			{
				nPrecision = va_arg(argList, int);
				lpsz++;
			}
			else
			{
				nPrecision = _ttoi(lpsz);
				for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz++);
			}
			assert_param(nPrecision >= 0);
		}

		// should be on type modifier or specifier
		int nModifier = 0;
		if (_tcsncmp(lpsz, _T("I64"), 3) == 0)
		{
			lpsz += 3;
			nModifier = FORCE_INT64;
#if !defined(_X86_) && !defined(_ALPHA_)
			// __int64 is only available on X86 and ALPHA platforms
			ASSERT(false);
#endif
		}
		else
		{
			switch (*lpsz)
			{
				// modifiers that affect size
			case 'h':
				nModifier = FORCE_ANSI;
				lpsz++;
				break;
			case 'l':
				nModifier = FORCE_UNICODE;
				lpsz++;
				break;

				// modifiers that do not affect size
			case 'F':
			case 'N':
			case 'L':
				lpsz++;
				break;
			}
		}

		// now should be on specifier
		switch (*lpsz | nModifier)
		{
			// single characters
		case 'c':
		case 'C':
			nItemLen = 2;
			va_arg(argList, TCHAR_ARG);
			break;
		case 'c'|FORCE_ANSI:
		case 'C'|FORCE_ANSI:
			nItemLen = 2;
			va_arg(argList, CHAR_ARG);
			break;
		case 'c'|FORCE_UNICODE:
		case 'C'|FORCE_UNICODE:
			nItemLen = 2;
			va_arg(argList, WCHAR_ARG);
			break;

			// strings
		case 's':
			{
				char* pstrNextArg = va_arg(argList, char*);
				if (pstrNextArg == NULL)
					nItemLen = 6;  // "(null)"
				else
				{
					nItemLen = strlen(pstrNextArg);
					nItemLen = max(1, nItemLen);
				}
			}
			break;

		case 'S':
			{
				char* pstrNextArg = va_arg(argList, char*);
				if (pstrNextArg == NULL)
					nItemLen = 6; // "(null)"
				else
				{
					nItemLen = strlen(pstrNextArg);
					nItemLen = max(1, nItemLen);
				}
			}
			break;

		case 's'|FORCE_ANSI:
		case 'S'|FORCE_ANSI:
			{
				LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
				if (pstrNextArg == NULL)
					nItemLen = 6; // "(null)"
				else
				{
					nItemLen = lstrlenA(pstrNextArg);
					nItemLen = max(1, nItemLen);
				}
			}
			break;

		case 's'|FORCE_UNICODE:
		case 'S'|FORCE_UNICODE:
			{
				char* pstrNextArg = va_arg(argList, char*);
				if (pstrNextArg == NULL)
					nItemLen = 6; // "(null)"
				else
				{
					nItemLen = wcslen(pstrNextArg);
					nItemLen = max(1, nItemLen);
				}
			}
			break;
		}

		// adjust nItemLen for strings
		if (nItemLen != 0)
		{
			if (nPrecision != 0) nItemLen = min(nItemLen, nPrecision);
			nItemLen = max(nItemLen, nWidth);
		}
		else
		{
			switch (*lpsz)
			{
				// integers
			case 'd':
			case 'i':
			case 'u':
			case 'x':
			case 'X':
			case 'o':
				if (nModifier & FORCE_INT64)
					va_arg(argList, __int64);
				else
					va_arg(argList, int);
				nItemLen = 32;
				nItemLen = max(nItemLen, nWidth+nPrecision);
				break;

			case 'e':
			case 'g':
			case 'G':
				va_arg(argList, DOUBLE_ARG);
				nItemLen = 128;
				nItemLen = max(nItemLen, nWidth+nPrecision);
				break;

			case 'f':
				{
					double f;
					char* pszTemp;

					// 312 == strlen("-1+(309 zeroes).")
					// 309 zeroes == max precision of a double
					// 6 == adjustment in case precision is not specified,
					//   which means that the precision defaults to 6
					pszTemp = (char*)_alloca(max(nWidth, 312+nPrecision+6));

					f = va_arg(argList, double);
					_stprintf( pszTemp, _T( "%*.*f" ), nWidth, nPrecision+6, f );
					nItemLen = _tcslen(pszTemp);
				}
				break;

			case 'p':
				va_arg(argList, void*);
				nItemLen = 32;
				nItemLen = max(nItemLen, nWidth+nPrecision);
				break;

				// no output
			case 'n':
				va_arg(argList, int*);
				break;

			default:
				assert_param(false);  // unknown formatting option
			}
		}

		// adjust nMaxLen for output nItemLen
		nMaxLen += nItemLen;
	}

	GetBuffer(nMaxLen);
	//VERIFY(_vstprintf(m_pchData, lpszFormat, argListSave) <= GetAllocLength());
	_vstprintf(m_pchData, lpszFormat, argListSave);
	ReleaseBuffer();

	va_end(argListSave);
}

void CString::Format(char* lpszFormat, ...)
{
	assert_param(lpszFormat);

	va_list argList;
	va_start(argList, lpszFormat);
	FormatV(lpszFormat, argList);
	va_end(argList);
}

void CString::ConcatCopy(int nSrc1Len, char* lpszSrc1Data,int nSrc2Len, char* lpszSrc2Data)
{
	int nNewLen = nSrc1Len + nSrc2Len;
	if (nNewLen != 0)
	{
		AllocBuffer(nNewLen);
		memcpy(m_pchData, lpszSrc1Data, nSrc1Len*sizeof(char));
		memcpy(m_pchData+nSrc1Len, lpszSrc2Data, nSrc2Len*sizeof(char));
	}
}

void CString::ConcatInPlace(int nSrcLen, char* lpszSrcData)
{
	if (nSrcLen == 0) return;

	if (GetData()->nRefs > 1 || GetData()->nDataLength + nSrcLen > GetData()->nAllocLength)
	{
		//动态分配
		CStringData* pOldData = GetData();
		ConcatCopy(GetData()->nDataLength, m_pchData, nSrcLen, lpszSrcData);
		assert_param(pOldData != NULL);
		CString::Release(pOldData);
	}
	else
	{
		//直接往后添加
		memcpy(m_pchData+GetData()->nDataLength, lpszSrcData, nSrcLen*sizeof(char));
		GetData()->nDataLength += nSrcLen;
		assert_param(GetData()->nDataLength <= GetData()->nAllocLength);
		m_pchData[GetData()->nDataLength] = '\0';
	}
}

const CString& CString::operator+=(char* lpsz)
{
	assert_param(lpsz == NULL || lpsz);
	ConcatInPlace(SafeStrlen(lpsz), lpsz);
	return *this;
}

const CString& CString::operator+=(char ch)
{
	ConcatInPlace(1, &ch);
	return *this;
}

const CString& CString::operator+=(const CString& string)
{
	ConcatInPlace(string.GetData()->nDataLength, string.m_pchData);
	return *this;
}


char* CString::GetBuffer(int nMinBufLength)
{
	assert_param(nMinBufLength >= 0);
	if (GetData()->nRefs > 1 || nMinBufLength > GetData()->nAllocLength)
	{ //重新动态分配
		CStringData* pOldData = GetData();
		int nOldLen = GetData()->nDataLength;   // AllocBuffer will tromp it
		if (nMinBufLength < nOldLen)
		nMinBufLength = nOldLen;
		AllocBuffer(nMinBufLength);
		memcpy(m_pchData, pOldData->data(), (nOldLen+1)*sizeof(char));
		GetData()->nDataLength = nOldLen;
		CString::Release(pOldData);
	}
	assert_param(GetData()->nRefs <= 1);
	assert_param(m_pchData != NULL);
	return m_pchData;
}

void CString::ReleaseBuffer(int nNewLength)
{
	CopyBeforeWrite();  //脱离共享数据块，

	if (nNewLength == -1)
	nNewLength = strlen(m_pchData); // zero terminated

	assert_param(nNewLength <= GetData()->nAllocLength);
	GetData()->nDataLength = nNewLength;
	m_pchData[nNewLength] = '\0';
}

char* CString::GetBufferSetLength(int nNewLength)
{
	assert_param(nNewLength >= 0);

	GetBuffer(nNewLength);
	GetData()->nDataLength = nNewLength;
	m_pchData[nNewLength] = '\0';
	return m_pchData;
}

void CString::FreeExtra()
{
	assert_param(GetData()->nDataLength <= GetData()->nAllocLength);
	if (GetData()->nDataLength != GetData()->nAllocLength)
	{
		CStringData* pOldData = GetData();
		AllocBuffer(GetData()->nDataLength);
		memcpy(m_pchData, pOldData->data(), pOldData->nDataLength*sizeof(char));
		assert_param(m_pchData[GetData()->nDataLength] == '\0');
		CString::Release(pOldData);
	}
	assert_param(GetData() != NULL);
}

char* CString::LockBuffer()
{
	char* lpsz = GetBuffer(0);
	GetData()->nRefs = -1;
	return lpsz;
}

void CString::UnlockBuffer()
{
	assert_param(GetData()->nRefs == -1);
	if (GetData() != _DataNil) GetData()->nRefs = 1;
}


int CString::Compare(char* lpsz) const
{
	assert_param(lpsz);
	return _tcscmp(m_pchData, lpsz);
}

int CString::CompareNoCase(char* lpsz) const
{
	assert_param(lpsz);
	return _tcsicmp(m_pchData, lpsz);
}

// CString::Collate is often slower than Compare but is MBSC/Unicode
//  aware as well as locale-sensitive with respect to sort order.
int CString::Collate(char* lpsz) const
{
	assert_param(lpsz);
	return _tcscoll(m_pchData, lpsz);
}


int CString::CollateNoCase(char* lpsz) const
{
	assert_param(lpsz);
	return _tcsicoll(m_pchData, lpsz);
}


char CString::GetAt(int nIndex) const
{
	assert_param(nIndex >= 0);
	assert_param(nIndex < GetData()->nDataLength);
	return m_pchData[nIndex];
}


char CString::operator[](int nIndex) const
{
	assert_param(nIndex >= 0);
	assert_param(nIndex < GetData()->nDataLength);
	return m_pchData[nIndex];
}

////////////////////////////////////////////////////////////////////////////////
CString operator+(const CString& string1, const CString& string2)
{
	CString s;
	s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData, string2.GetData()->nDataLength, string2.m_pchData);
	return s;
}

CString operator+(const CString& string, char* lpsz)
{
	assert_param(lpsz);
	CString s;
	s.ConcatCopy(string.GetData()->nDataLength, string.m_pchData, CString::SafeStrlen(lpsz), lpsz);
	return s;
}

CString operator+(char* lpsz, const CString& string)
{
	assert_param(lpsz);
	CString s;
	s.ConcatCopy(CString::SafeStrlen(lpsz), lpsz, string.GetData()->nDataLength, string.m_pchData);
	return s;
}

bool operator==(const CString& s1, const CString& s2) { return s1.Compare(s2) == 0; }
bool operator==(const CString& s1, char* s2) { return s1.Compare(s2) == 0; }
bool operator==(char* s1, const CString& s2) { return s2.Compare(s1) == 0; }
bool operator!=(const CString& s1, const CString& s2) { return s1.Compare(s2) != 0; }
bool operator!=(const CString& s1, char* s2) { return s1.Compare(s2) != 0; }
bool operator!=(char* s1, const CString& s2) { return s2.Compare(s1) != 0; }
bool operator<(const CString& s1, const CString& s2) { return s1.Compare(s2) < 0; }
bool operator<(const CString& s1, char* s2) { return s1.Compare(s2) < 0; }
bool operator<(char* s1, const CString& s2) { return s2.Compare(s1) > 0; }
bool operator>(const CString& s1, const CString& s2) { return s1.Compare(s2) > 0; }
bool operator>(const CString& s1, char* s2) { return s1.Compare(s2) > 0; }
bool operator>(char* s1, const CString& s2) { return s2.Compare(s1) < 0; }
bool operator<=(const CString& s1, const CString& s2) { return s1.Compare(s2) <= 0; }
bool operator<=(const CString& s1, char* s2) { return s1.Compare(s2) <= 0; }
bool operator<=(char* s1, const CString& s2) { return s2.Compare(s1) >= 0; }
bool operator>=(const CString& s1, const CString& s2) { return s1.Compare(s2) >= 0; }
bool operator>=(const CString& s1, char* s2) { return s1.Compare(s2) >= 0; }
bool operator>=(char* s1, const CString& s2) { return s2.Compare(s1) <= 0; }
