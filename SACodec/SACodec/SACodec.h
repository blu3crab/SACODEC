#ifndef _SACODEC_H_
#define _SACODEC_H_

typedef struct {
	DWORD fileSize;
	DWORD sigSize;
	DWORD sigSpacing;
	DWORD border;
	char  sig[ 256 ];
} CTL;

#define MAX_READ_WRITE_SIZE 4096

class SACodec
{
	private:
		HANDLE copyFile( const char*, CTL*, std::string & );
		void ReadINI( const char*, CTL*, char [ ] );
		void writeCTLElements( HANDLE, CTL*, const char* );
		int parseEquation( const char*, std::string, CTL* );
		int eval( std::string );
		int readCTLElements( HANDLE, const char*, const char* );
		int compareCrcs( HANDLE, const char* );
		
	public:
		SACodec( );
		~SACodec( );
		bool EncodeFile( const char* );
		bool DecodeFile( const char*, char*, char* );
};

#endif