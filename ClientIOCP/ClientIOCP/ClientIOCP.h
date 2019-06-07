#ifndef _CLIENT_IOCP_H_
#define _CLIENT_IOCP_H_

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <winsock2.h>
#include <vector>
#include <queue>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#define MAX_BUFFER_LEN 255
typedef void (*function_pointer)();

typedef boost::function<void(Packet*)>	fCallbackFunctor;
enum typePackage
{
	typeEcho,
	typeData,
	typeMessage,
	typeNone

};



class BuffPacket
{
public:
	BuffPacket()
	{
	};
	BuffPacket(const BuffPacket& pack)
	{
		m_buffer = pack.m_buffer;

	};
	typedef boost::shared_ptr<BuffPacket> sptr;

	bool CreatePackage(typePackage type, unsigned char* data, UINT nSize)
	{
		UINT typPackage = type;
		UINT sizeOfPackage = sizeof(UINT) + sizeof(UINT) + nSize;
		typPackage = htonl(typPackage);
		sizeOfPackage = htonl(sizeOfPackage);
		if (m_buffer.empty())
			Append(reinterpret_cast<const BYTE*>(&typPackage), sizeof(UINT));
		Append(reinterpret_cast<const BYTE*>(&sizeOfPackage), sizeof(UINT));
		Append(data, nSize);
		return true;
	}
	bool CreateInfoPackage(typePackage type)
	{
		UINT typPackage = type;
		UINT sizeOfPackage = sizeof(UINT) + sizeof(UINT);
		typPackage = htonl(typPackage);
		sizeOfPackage = htonl(sizeOfPackage);
		if (m_buffer.empty())
			Append(reinterpret_cast<const BYTE*>(&typPackage), sizeof(UINT));
		Append(reinterpret_cast<const BYTE*>(&sizeOfPackage), sizeof(UINT));

		return true;
	}

	UINT getPackageType()
	{


		UINT type = typeNone;


		if (m_buffer.size() >= sizeof(UINT) + sizeof(UINT))
		{
			memmove(&type, GetBuffer(), sizeof(UINT));

			type = ntohl(type);
		}
		return type;
	}
	UINT getPackageSize()
	{


		UINT size = 0;
		if (m_buffer.size() >= sizeof(UINT) + sizeof(UINT))
		{
			memmove(&size, GetBuffer() + sizeof(UINT), sizeof(UINT));
			size = ntohl(size);
		}
		return size;
	}

	void GetBuffer(unsigned char* data, UINT maxSize)
	{
		UINT sizeOfDate = getPackageSize();
		if (sizeOfDate > sizeof(UINT) + sizeof(UINT) && sizeOfDate <= maxSize)
		{
			for (UINT i = sizeof(UINT) + sizeof(UINT), j = 0; i < sizeOfDate && i < maxSize; i++, j++)
			{

				data[j] = m_buffer[i];
			}

		}

	}
	BuffPacket(const char* buffer, const int size)
	{

		Append(reinterpret_cast<const unsigned char*>(buffer), size);
	};

	BuffPacket(const unsigned char* buffer, const int size)
	{
		Append(buffer, size);
	}
	void  Append(const unsigned char* dataBuffer, const int size)
	{
		m_buffer.insert(m_buffer.end(), dataBuffer, dataBuffer + size);
	}

	unsigned int BuffSize() { return m_buffer.size(); }
	bool IsEmpty()const { return m_buffer.empty(); }
	unsigned char* GetBuffer()
	{
		return &m_buffer.front();
	}
	void Clear()
	{
		m_buffer.clear();
	}

	std::vector<unsigned char> Buffer() const { return m_buffer; }
privete:
	std::vector<unsigned char> m_buffer;
};




struct ClientSession
{
public:
	WSABUF* m_pwbuf;
	int m_nThreadNo;
	int m_nNoOfSends;
	SOCKET m_Socket;
	char m_szBuffer[MAX_BUFFER_LEN];
	int  m_totalBytesSent;
	int  m_bytesSent;
	BuffPacket::sptr  m_SendBuffer;


	int  m_totalBytesRead;
	int  m_bytesRead;
	BuffPacket::sptr  m_ReadBuffer;
	std::queue<BuffPacket::sptr> packQueue;
	std::queue<BuffPacket::sptr> packRecive;
private:
	boost::mutex g_Client_pages_mutex;
public:


	ClientSession()
	{
		m_pwbuf = new WSABUF;


		m_Socket = SOCKET_ERROR;

		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);

		m_pwbuf->buf = m_szBuffer;
		m_pwbuf->len = MAX_BUFFER_LEN;

		SetTotalBytesSent(0);
		SetBytesSent(0);
		SetTotalBytesRead(0);
		SetBytesRead(0);
		m_ReadBuffer = BuffPacket::sptr(new BuffPacket());
		m_SendBuffer = BuffPacket::sptr(new BuffPacket());
	}

	int TotalBytesSent() const { return m_totalBytesSent; }
	void SetTotalBytesSent(int val) { m_totalBytesSent = val; }
	int BytesSent() const { return m_bytesSent; }
	void SetBytesSent(int val) { m_bytesSent = val; }
	int TotalBytesRead() const { return m_totalBytesRead; }
	void SetTotalBytesRead(int val) { m_totalBytesRead = val; }
	int BytesRead() const { return m_bytesRead; }
	void SetBytesRead(int val) { m_bytesRead = val; }

	Packet* GetPacket()
	{
		Packet* pck = new Packet;


		if (m_ReadBuffer.get() && m_ReadBuffer->getPackageType() != typeEcho)
		{
			boost::mutex::scoped_lock threadLock(g_Client_pages_mutex);
			int sizepack = m_ReadBuffer->getPackageSize();
			pck->data = new unsigned char[sizepack + 1];
			ZeroMemory(pck->data, sizepack);
			m_ReadBuffer->GetBuffer(pck->data, m_ReadBuffer->getPackageSize());

			pck->length = sizepack;
			m_ReadBuffer = BuffPacket::sptr();

			return pck;

		}

		return NULL;
	}

	void addReciveQueue()
	{
		boost::mutex::scoped_lock threadLock(g_Client_pages_mutex);
		if (m_ReadBuffer.get())
		{
			packRecive.push(m_ReadBuffer);
			m_ReadBuffer = BuffPacket::sptr();

		}

	}

	BuffPacket::sptr  ReciveNext()
	{
		boost::mutex::scoped_lock threadLock(g_Client_pages_mutex);
		if (packRecive.size() > 0)
		{
			BuffPacket::sptr PackageBuffer = packRecive.front();
			packRecive.pop();
			if (PackageBuffer.get())
			{

				return PackageBuffer;
			}
		}




		return BuffPacket::sptr();
	}

	void addQueue(BuffPacket::sptr package)
	{
		boost::mutex::scoped_lock threadLock(g_Client_pages_mutex);
		packQueue.push(package);

	}

	bool GetNext()
	{
		boost::mutex::scoped_lock threadLock(g_Client_pages_mutex);
		if (packQueue.size() > 0)
		{
			BuffPacket::sptr PackageBuffer = packQueue.front();
			packQueue.pop();
			if (PackageBuffer.get())
			{
				m_SendBuffer = PackageBuffer;
				m_pwbuf->buf = reinterpret_cast<char*>(m_SendBuffer->GetBuffer());;
				m_pwbuf->len = m_SendBuffer->BuffSize();
				SetBytesSent(0);
				SetTotalBytesSent(m_SendBuffer->BuffSize());
				return true;
			}
		}

		m_SendBuffer = BuffPacket::sptr(new BuffPacket());
		m_SendBuffer->CreateInfoPackage(typeEcho);
		m_pwbuf->buf = reinterpret_cast<char*>(m_SendBuffer->GetBuffer());
		m_pwbuf->len = m_SendBuffer->BuffSize();
		// CreateInfoPackage
		SetBytesSent(0);
		SetTotalBytesSent(m_SendBuffer->BuffSize());


		return true;
	}

	void ZeroBuffer()
	{
		ZeroMemory(m_szBuffer, MAX_BUFFER_LEN);
	}

	void SetWSABUFLength(int nLength)
	{
		m_pwbuf->len = nLength;
	}

	int GetWSABUFLength()
	{
		return m_pwbuf->len;
	}

	WSABUF* GetWSABUFPtr()
	{
		return m_pwbuf;
	}


	void ResetWSABUF()
	{
		ZeroBuffer();
		m_pwbuf->buf = m_szBuffer;
		m_pwbuf->len = MAX_BUFFER_LEN;
	}

};















#endif