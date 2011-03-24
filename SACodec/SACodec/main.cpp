/////////////////////////////////////////////////////////////////
// SACODEC - 28FEB11
// author: nat
// fiddling...
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
void writeCTLElements( HANDLE, CTL* );
int parseEquation( const char*, std::string, CTL* );
int eval( std::string buffer );

int main( int argc, char* argv[ ] ) 
{
	switch( argc )
	{
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
	//write rest of sig according to equation
	writeCTLElements( newFile, &controlSig );

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
				iniBuffer = iniBuffer.substr( iniLocations + strlen( "[Equation]=" ) );
					
				controlSig->sigSpacing = parseEquation( filePath, iniBuffer, controlSig );
			}
		}

		iniStream.close( );
	}

	return;
}

void writeCTLElements( HANDLE newFile, CTL* controlSig)
{
	int currentBlock = 0, count = 0;
	DWORD bytesRead = MAX_READ_WRITE_SIZE, bytesWritten = 0, fpPos[ 4 ][ 2 ] = { 0 };
	char readBuffer[ MAX_READ_WRITE_SIZE ] = { 0 };
	const char ctlIdentifier[ ] = "DEADBEEF";
	bool found = false;

	SetFilePointer( newFile, 0, NULL, FILE_BEGIN );

	while( bytesRead == MAX_READ_WRITE_SIZE && found == false )
	{
		if( ReadFile( newFile, readBuffer, MAX_READ_WRITE_SIZE, &bytesRead, NULL ) )
		{
			for( int i = 0; i < (signed)( bytesRead - 20 ); i++ )
			{
				for( int j = 0; j < 20; j++ )
				{
					if( readBuffer[ i + j ] == 0x00 )
						count++;
				}
				if( count == 20 )
				{
					SetFilePointer( newFile, currentBlock + i, NULL, FILE_BEGIN );
					found = true;
					break;
				}
				else
					count = 0;
			}
		}
		currentBlock += bytesRead;
	}

	if( found == false )
		SetFilePointer( newFile, controlSig->border, NULL, FILE_BEGIN );

	WriteFile( newFile, ctlIdentifier, sizeof( ctlIdentifier ), &bytesWritten, NULL );
	char ctlBuf[ 65 ] = { 0 };
	_itoa_s( GetFileSize( newFile, NULL ) / 6, ctlBuf, sizeof( ctlBuf ), 10 );
	strcat_s( ctlBuf, sizeof( ctlBuf ), "!" );
	WriteFile( newFile, ctlBuf, strlen( ctlBuf ), &bytesWritten, NULL );

	//write CTL in the file at the spacing got after DEADBEEF
	for( int i = 1; i < 5; i++ )
	{
		fpPos[ i - 1 ][ 0 ] = SetFilePointer( newFile, ( GetFileSize( newFile, NULL ) / 6 ) * i, NULL, FILE_BEGIN );
		switch( i )
		{
		case 1:
			_itoa_s( controlSig->fileSize, ctlBuf, sizeof( ctlBuf ), 10 );
			strcat_s( ctlBuf, sizeof( ctlBuf ), "!" );
			break;
		case 2:
			_itoa_s( controlSig->sigSize, ctlBuf, sizeof( ctlBuf ), 10 );
			strcat_s( ctlBuf, sizeof( ctlBuf ), "!" );
			break;
		case 3:
			_itoa_s( controlSig->sigSpacing, ctlBuf, sizeof( ctlBuf ), 10 );
			strcat_s( ctlBuf, sizeof( ctlBuf ), "!" );
			break;
		case 4:
			_itoa_s( controlSig->border, ctlBuf, sizeof( ctlBuf ), 10 );
			strcat_s( ctlBuf, sizeof( ctlBuf ), "!" );
		}
		WriteFile( newFile, ctlBuf, strlen( ctlBuf ), &bytesWritten, NULL );
		fpPos[ i - 1 ][ 1 ] = bytesWritten;
	}
	
	//use equation to write SIG
	DWORD sigFpPos = 0;
	char temp[ 256 ] = { 0 };
	std::string s1;

	strcpy_s( temp, sizeof( temp ), controlSig->sig );
	for( int i = 1; i < (signed)( strlen( controlSig->sig ) ) + 1; i++ )
	{
		sigFpPos = SetFilePointer( newFile, controlSig->sigSpacing * i, NULL, FILE_BEGIN );
		//check if sigFpPos is within fpPos[ ], if it is, increase sigFpPos to make sure not to overwrite
		for( int j = 0; j < 4; j++ )
		{
			if( sigFpPos > fpPos[ j ][ 0 ] && sigFpPos < fpPos[ j ][ 0 ] + fpPos[ j ][ 1 ] )
			{
				sigFpPos = SetFilePointer( newFile, fpPos[ j ][ 0 ] + fpPos[ j ][ 1 ], NULL, FILE_BEGIN );
				break;
			}
		}
		//write the element
		WriteFile( newFile, temp, 1, &bytesWritten, NULL );

		s1 = temp;
		strcpy_s( temp, sizeof( temp ), s1.substr( 1 ).c_str( ) );	
	}
}
//This is really ugly... but it works. TODO: CLEAN UP
int parseEquation( const char* filePath, std::string iniBuffer, CTL *controlSig )
{
	char iniBuf[ 65 ] = { 0 };
	std::string temp, temp2;

	//replace our variables
	_itoa_s( controlSig->fileSize, iniBuf, sizeof( iniBuf ), 10 );
	while( iniBuffer.find( "FILE_SIZE" ) != std::string::npos )
		iniBuffer.replace( iniBuffer.find( "FILE_SIZE" ), strlen( "FILE_SIZE" ), iniBuf );

	_itoa_s( strlen( controlSig->sig ), iniBuf, sizeof( iniBuf ), 10 );
	while( iniBuffer.find( "SIG_SIZE" ) != std::string::npos )
		iniBuffer.replace( iniBuffer.find( "SIG_SIZE" ), strlen( "SIG_SIZE" ), iniBuf );

	_itoa_s( controlSig->border, iniBuf, sizeof( iniBuf ), 10 );
	while( iniBuffer.find( "BORDER" ) != std::string::npos )
		iniBuffer.replace( iniBuffer.find( "BORDER" ), strlen( "BORDER" ), iniBuf );

	_itoa_s( strlen( filePath ), iniBuf, sizeof( iniBuf ), 10 );
	while( iniBuffer.find( "CUR_INI_PATH" ) != std::string::npos )
		iniBuffer.replace( iniBuffer.find( "CUR_INI_PATH" ), strlen( "CUR_INI_PATH" ), iniBuf );

	//eval () first
	while( iniBuffer.find( "(" ) != std::string::npos )
	{
		temp = iniBuffer.substr( iniBuffer.find( "(" ) + 1, iniBuffer.find( ")" ) - iniBuffer.find( "(" ) - 1  );
		_itoa_s( eval( temp ), iniBuf, sizeof( iniBuf ), 10 );
		iniBuffer.replace( iniBuffer.find( "(" ), iniBuffer.find( ")" ) - iniBuffer.find( "(" ) + 1, iniBuf );
	}

	//eval *,/,+,-
	while( iniBuffer.find( "*" ) != std::string::npos )
	{
		temp = iniBuffer.substr( 0, iniBuffer.find( "*" ) );
		temp2 = iniBuffer.substr( iniBuffer.find( "*" ) );
		if( temp.find_last_of( "+-*/" ) != std::string::npos )
			temp = temp.substr( temp.find_last_of( "+-*/" ) );
		if( temp2.substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
			temp2 = temp2.substr( 0, temp2.substr( 1 ).find_first_of( "+-*/" ) + 1 );

		_itoa_s( eval( temp.append( temp2 ) ), iniBuf, sizeof( iniBuf ), 10 );

		if( iniBuffer.substr( 0, iniBuffer.find( "*" ) ).find_last_of( "+-*/" ) != std::string::npos )
		{
			if( iniBuffer.substr( iniBuffer.find( "*" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "*" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "*" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "*" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "*" ) ).find_last_of( "+-*/" ), iniBuf );
		}
		else
		{
			if( iniBuffer.substr( iniBuffer.find( "*" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "*" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "*" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "*" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "*" ) ).find_last_of( "+-*/" ), iniBuf );
		}	
	}


	while( iniBuffer.find( "/" ) != std::string::npos )
	{
		temp = iniBuffer.substr( 0, iniBuffer.find( "/" ) );
		temp2 = iniBuffer.substr( iniBuffer.find( "/" ) );
		if( temp.find_last_of( "+-*/" ) != std::string::npos )
			temp = temp.substr( temp.find_last_of( "+-*/" ) );
		if( temp2.substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
			temp2 = temp2.substr( 0, temp2.substr( 1 ).find_first_of( "+-*/" ) + 1 );

		_itoa_s( eval( temp.append( temp2 ) ), iniBuf, sizeof( iniBuf ), 10 );

		if( iniBuffer.substr( 0, iniBuffer.find( "/" ) ).find_last_of( "+-*/" ) != std::string::npos )
		{
			if( iniBuffer.substr( iniBuffer.find( "/" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "/" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "/" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "/" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "/" ) ).find_last_of( "+-*/" ), iniBuf );
		}
		else
		{
			if( iniBuffer.substr( iniBuffer.find( "/" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "/" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "/" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "/" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "/" ) ).find_last_of( "+-*/" ), iniBuf );
		}	
	}

	while( iniBuffer.find( "+" ) != std::string::npos )
	{
		temp = iniBuffer.substr( 0, iniBuffer.find( "+" ) );
		temp2 = iniBuffer.substr( iniBuffer.find( "+" ) );
		if( temp.find_last_of( "+-*/" ) != std::string::npos )
			temp = temp.substr( temp.find_last_of( "+-*/" ) );
		if( temp2.substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
			temp2 = temp2.substr( 0, temp2.substr( 1 ).find_first_of( "+-*/" ) + 1 );

		_itoa_s( eval( temp.append( temp2 ) ), iniBuf, sizeof( iniBuf ), 10 );

		if( iniBuffer.substr( 0, iniBuffer.find( "+" ) ).find_last_of( "+-*/" ) != std::string::npos )
		{
			if( iniBuffer.substr( iniBuffer.find( "+" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "+" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "+" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "+" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "+" ) ).find_last_of( "+-*/" ), iniBuf );
		}
		else
		{
			if( iniBuffer.substr( iniBuffer.find( "+" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "+" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "+" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "+" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "+" ) ).find_last_of( "+-*/" ), iniBuf );
		}	
	}

	while( iniBuffer.find( "-" ) != std::string::npos )
	{
		temp = iniBuffer.substr( 0, iniBuffer.find( "-" ) );
		temp2 = iniBuffer.substr( iniBuffer.find( "-" ) );
		if( temp.find_last_of( "+-*/" ) != std::string::npos )
			temp = temp.substr( temp.find_last_of( "+-*/" ) );
		if( temp2.substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
			temp2 = temp2.substr( 0, temp2.substr( 1 ).find_first_of( "+-*/" ) + 1 );

		_itoa_s( eval( temp.append( temp2 ) ), iniBuf, sizeof( iniBuf ), 10 );

		if( iniBuffer.substr( 0, iniBuffer.find( "-" ) ).find_last_of( "+-*/" ) != std::string::npos )
		{
			if( iniBuffer.substr( iniBuffer.find( "-" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "-" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "-" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "-" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "-" ) ).find_last_of( "+-*/" ), iniBuf );
		}
		else
		{
			if( iniBuffer.substr( iniBuffer.find( "-" ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( "-" ) ).substr( 1 ).find_first_of( "+-*/" ) + iniBuffer.find( "-" ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( "-" ) ).find_last_of( "+-*/" ) + 1, iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( "-" ) ).find_last_of( "+-*/" ), iniBuf ); 
		}	
	}

	return atoi( iniBuffer.c_str( ) );
}

int eval( std::string buffer )
{
	if( buffer.find( "/" ) != std::string::npos )
		return atoi( buffer.substr( 0, buffer.find( "/" ) ).c_str( ) ) / atoi( buffer.substr( buffer.find( "/" ) + 1  ).c_str( ) );
	else if( buffer.find( "*" ) != std::string::npos )
		return atoi( buffer.substr( 0, buffer.find( "*" ) ).c_str( ) ) * atoi( buffer.substr( buffer.find( "*" ) + 1  ).c_str( ) );
	else if( buffer.find( "+" ) != std::string::npos )
		return atoi( buffer.substr( 0, buffer.find( "+" ) ).c_str( ) ) + atoi( buffer.substr( buffer.find( "+" ) + 1  ).c_str( ) );
	else if( buffer.find( "-" ) != std::string::npos )
		return atoi( buffer.substr( 0, buffer.find( "-" ) ).c_str( ) ) - atoi( buffer.substr( buffer.find( "-" ) + 1  ).c_str( ) );
	else
		return 0;
}
