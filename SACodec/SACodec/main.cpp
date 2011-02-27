/////////////////////////////////////////////////////////////////
// SACODEC - 28FEB11
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>

#define MAX_READ_WRITE_SIZE 4096

typedef struct {
	DWORD fileSize;
	DWORD sigSize;
	DWORD sigSpacing;
	DWORD border;
	char  sig[ 256 ];
} CTL;

bool EncodeFile( const char* );
bool DecodeFile( const char* );
HANDLE copyFile( const char*, CTL* );
void ReadINI( const char*, CTL* );

int main( int argc, char* argv[ ] ) 
{
	switch( argc )
	{
	case 2:
		if( strcmp( argv[ 1 ], "-help" ) == 0 )
			std::cout << "Here goes help" << std::endl;
		else
			std::cout << "Here goes help" << std::endl;

		break;

	case 3:
		if( strcmp( argv[ 1 ], "-e" ) == 0 || strcmp( argv[ 1 ], "-E" ) == 0 )
		{
			//TODO: check for directory as filename
			if( EncodeFile( argv[ 2 ] ) != 0 )
			{
				std::cout << "Problem encoding the file." << std::endl;
			}
			else
			{
				std::cout << " --- " << std::endl << "File " << argv[ 2 ]<< " encoded successfully" << std::endl;
			}
		}
		else if( strcmp( argv[ 1 ], "-d" ) == 0 || strcmp( argv[ 1 ], "-D" ) == 0 )
		{
			//TODO: check for directory, check for problems
			//DecodeFile( argv[ 2 ] );
			//TODO: check for directory as filename
			if( DecodeFile( argv[ 2 ] ) != 0 )
			{
				std::cout << "Problem decoding the file." << std::endl;
			}
			else
			{
				std::cout << " --- " << std::endl << "File " << argv[ 2 ]<< " decoded successfully" << std::endl;
			}
		}
		else
			std::cout << "Invalid arguments. Please type -help for details." << std::endl;

		break;

	default:
		std::cout << "Here goes help" << std::endl;

		break;
	}

	return 0;
}

bool EncodeFile( const char* filePath )
{
	CTL controlSig = { 0 };
	HANDLE newFile = 0;
	DWORD fpPos = 0, bytesWritten = 0;
	char buffer[ MAX_PATH ] = { 0 };

	//Create+Copy file to fileName.SAC.original extension
	newFile = copyFile( filePath, &controlSig );
	if( newFile == NULL )
		return 1;

	//Read CTL elements from INI
	GetModuleFileName( NULL, buffer, MAX_PATH );
	for( int i = strlen( buffer ); buffer[ i ] != '\\'; i-- )
		buffer[ i ] = 0;
	strcat_s( buffer, sizeof( buffer ), "SACodec.ini" );

	ReadINI( buffer, &controlSig );

	//sig scan for 00's array, throw DEADBEEF sig + spacing between CTL elements there
	//if you can't find it, throw it in a random place a bit from the header
	fpPos = SetFilePointer( newFile, 0, NULL, FILE_BEGIN );

	//write CTL in the file at the spacing got after DEADBEEF
	//use equation to write SIG
	//grab CRC, create a binary file holding this

	CloseHandle( newFile );

	return 0;
}

bool DecodeFile( const char* filePath )
{

	return 0;
}

HANDLE copyFile( const char* filePath, CTL *controlSig )
{
	std::string tempFileName = filePath;
	HANDLE originalFile = 0, newFile = 0;
	DWORD fpPos = 0, bytesRead = MAX_READ_WRITE_SIZE, bytesWritten = 0;
	char buffer[ MAX_READ_WRITE_SIZE ] = { 0 };

	tempFileName.insert( tempFileName.find_last_of( "." ) + 1, "SAC." );
	
	originalFile = CreateFile( reinterpret_cast< LPCSTR > ( filePath ), GENERIC_READ, 0, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	
	if( originalFile == INVALID_HANDLE_VALUE )
	{
		std::cout << "Could not open the original file, exiting." << std::endl;
		return NULL;
	}

	newFile = CreateFile( reinterpret_cast< LPCSTR> ( tempFileName.c_str( ) ), GENERIC_READ | GENERIC_WRITE, 0,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

	if( newFile == INVALID_HANDLE_VALUE )
	{
		std::cout << "Could not create new file, check permissions." << std::endl;
		return NULL;
	}
	
	fpPos = SetFilePointer( newFile, 0, NULL, FILE_BEGIN );
	if( fpPos == INVALID_SET_FILE_POINTER )
	{
		std::cout << "Could not create new file, check permissions." << std::endl;
		return NULL;
	}

	while( bytesRead == MAX_READ_WRITE_SIZE )
	{
		if( ReadFile( originalFile, buffer, MAX_READ_WRITE_SIZE, &bytesRead, NULL ) )
		{
			WriteFile( newFile, buffer, bytesRead, &bytesWritten, NULL );
		}
	}

	if( controlSig != NULL )
		controlSig->fileSize = GetFileSize( originalFile, NULL );

	CloseHandle( originalFile );

	return newFile;
}

//TODO: parse equation
void ReadINI( const char* filePath, CTL *controlSig )
{
	std::ifstream iniStream;
	std::string iniBuffer;
	size_t iniLocations = 0;

	if( controlSig == NULL )
		return;

	iniStream.open( filePath, std::ios::in );
	if( iniStream.is_open( ) )	
	{
		while( iniStream.good( ) )
		{
			std::getline( iniStream, iniBuffer );
			iniLocations = iniBuffer.find( "[Signature]" );
			if( iniLocations != std::string::npos )
			{
				strcpy_s( controlSig->sig, sizeof( controlSig->sig ), iniBuffer.substr( iniLocations + 
					strlen( "[Signature]=" ) ).c_str( ) );

				controlSig->sigSize = strlen( controlSig->sig );
			}
			
			iniLocations = iniBuffer.find( "[Border]" );
			if( iniLocations != std::string::npos )
			{
				controlSig->border = atoi( iniBuffer.substr( iniLocations + strlen( "[Border]=" ) ).c_str( ) );
			}

			iniLocations = iniBuffer.find( "[Equation]" );
			if( iniLocations != std::string::npos )
			{
				// TODO: Parse the equation
				iniBuffer = iniBuffer.substr( iniLocations + strlen( "[Equation]=" ) );
				
				
				//controlSig->sigSpacing=/
			}
		}

		iniStream.close( );
	}

	return;
}