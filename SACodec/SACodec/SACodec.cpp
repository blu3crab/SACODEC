#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include "crc32.h"
#include "SACodec.h"

SACodec::SACodec( )
{
	//empty for now
}
SACodec::~SACodec( )
{
	//empty for now
}

bool SACodec::EncodeFile( const char* filePath )
{
	CTL controlSig = { 0 };
	HANDLE newFile = 0, crcFile = 0;
	char buffer[ MAX_PATH ] = { 0 }, ctlIdentifier[ 256 ] = { 0 };
	DWORD bytesWritten = 0;
	std::string fileName;

	//Create+Copy file to fileName.SAC.original extension
	newFile = copyFile( filePath, &controlSig, fileName );
	if( newFile == NULL )
		return 1;

	//Read CTL elements from INI
	GetModuleFileName( NULL, buffer, MAX_PATH );
	for( int i = strlen( buffer ); buffer[ i ] != '\\'; i-- )
		buffer[ i ] = 0;
	strcat_s( buffer, sizeof( buffer ), "SACodec.ini" );

	ReadINI( buffer, &controlSig, ctlIdentifier );

	//sig scan for 00's array, throw DEADBEEF sig + spacing between CTL elements there
	//if you can't find it, throw it in a random place a bit from the header
	//write rest of sig according to equation
	writeCTLElements( newFile, &controlSig, ctlIdentifier );

	//grab CRC, create a binary file holding this
	crcFile = CreateFile( reinterpret_cast< LPCSTR > ( fileName.substr( 0, fileName.find_last_of( "." ) ).c_str( ) ), 
		GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	
	if( crcFile == INVALID_HANDLE_VALUE )
	{
		std::cout << "Could not create crc file, check permissions." << std::endl;
		return NULL;
	}

	SetFilePointer( crcFile, 0, NULL, FILE_BEGIN );
	SetFilePointer( newFile, 0, NULL, FILE_BEGIN );
	_itoa_s( genCRC32( newFile ), buffer, sizeof( buffer ), 16 );
	WriteFile( crcFile, buffer, strlen( buffer ), &bytesWritten, NULL );
	CloseHandle( crcFile );

	CloseHandle( newFile );

	return 0;
}

bool SACodec::DecodeFile( const char* filePath, char* key, char* sig)
{
	HANDLE originalFile = 0;
	CTL tempCTL = { 0 };

	originalFile = CreateFile( reinterpret_cast< LPCSTR > ( filePath ), GENERIC_READ, 0, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	
	if( compareCrcs( originalFile, filePath ) )
		return 1;

	//either use key/sig from input, or read ini for key
	if( key == NULL || sig == NULL )
	{
		char iniLoc[ 256 ] = { 0 }, tempKey[ 256 ] = { 0 }, tempSig[ 256 ] = { 0 };

		GetModuleFileName( NULL, iniLoc, MAX_PATH );
		for( int i = strlen( iniLoc ); iniLoc[ i ] != '\\'; i-- )
			iniLoc[ i ] = 0;
		strcat_s( iniLoc, sizeof( iniLoc ), "SACodec.ini" );

		std::ifstream iniStream;
		std::string iniBuffer;
		size_t iniLocations = 0;

		iniStream.open( iniLoc, std::ios::in );
		if( iniStream.is_open( ) )
		{
			while( iniStream.good( ) )
			{
				std::getline( iniStream, iniBuffer );	

				iniLocations = iniBuffer.find( "[Signature]" );
				if( iniLocations != std::string::npos )
				{
					strcpy_s( tempSig, 256, iniBuffer.substr( iniLocations + 
						strlen( "[Signature]=" ) ).c_str( ) );
				}
		
				iniLocations = iniBuffer.find( "[Identifier]" );
				if( iniLocations != std::string::npos )
				{
					strcpy_s( tempKey, 256, iniBuffer.substr( iniLocations + 
						strlen( "[Identifier]=" ) ).c_str( ) );
				}
			}

			iniStream.close( );
		}
		
		if( key == NULL )
			key = tempKey;
		if( sig == NULL )
			sig = tempSig;
	}	

	//scan for key within file
	if( readCTLElements( originalFile, key, sig ) )
		return 1;

	CloseHandle( originalFile );

	return 0;
}

HANDLE SACodec::copyFile( const char* filePath, CTL *controlSig, std::string &tempFileName )
{
	tempFileName = filePath;
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

	newFile = CreateFile( reinterpret_cast< LPCSTR > ( tempFileName.c_str( ) ), GENERIC_READ | GENERIC_WRITE, 0,
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
void SACodec::ReadINI( const char* filePath, CTL *controlSig, char ctlIdentifier[ 256 ] )
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

			iniLocations = iniBuffer.find( "[Identifier]" );
			if( iniLocations != std::string::npos )
			{
				strcpy_s( ctlIdentifier, 256, iniBuffer.substr( iniLocations + 
					strlen( "[Identifier]=" ) ).c_str( ) );
			}
		}

		iniStream.close( );
	}

	return;
}

void SACodec::writeCTLElements( HANDLE newFile, CTL* controlSig, const char* ctlIdentifier )
{
	int currentBlock = 0, count = 0;
	DWORD bytesRead = MAX_READ_WRITE_SIZE, bytesWritten = 0, fpPos[ 4 ][ 2 ] = { 0 };
	char readBuffer[ MAX_READ_WRITE_SIZE ] = { 0 };
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

	char ctlBuf[ 256 ] = { 0 };
	ctlBuf[ 0 ] = '!';
	strcat_s( ctlBuf, sizeof( ctlBuf ), ctlIdentifier );
	strcat_s( ctlBuf, sizeof( ctlBuf ), "!" );
	WriteFile( newFile, ctlBuf, strlen( ctlBuf ) + 1, &bytesWritten, NULL );
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
int SACodec::parseEquation( const char* filePath, std::string iniBuffer, CTL *controlSig )
{
	char iniBuf[ 256 ] = { 0 }, *vars[ 4 ] = { "FILE_SIZE", "SIG_SIZE", "BORDER", "CUR_INI_PATH" }, 
		*operations[ 4 ] = { "*", "/", "+", "-" };
	std::string temp, temp2;

	//replace our variables
	for( int i = 0; i < 4; i++ )
	{
		switch( i )
		{
		case 0:
			_itoa_s( controlSig->fileSize, iniBuf, sizeof( iniBuf ), 10 );
			break;
		case 1:
			_itoa_s( strlen( controlSig->sig ), iniBuf, sizeof( iniBuf ), 10 );
			break;
		case 2:
			_itoa_s( controlSig->border, iniBuf, sizeof( iniBuf ), 10 );
			break;
		case 3:
			_itoa_s( strlen( filePath ), iniBuf, sizeof( iniBuf ), 10 );
		}

		while( iniBuffer.find( vars[ i ] ) != std::string::npos )
			iniBuffer.replace( iniBuffer.find( vars[ i ] ), strlen( vars[ i ] ), iniBuf );
	}

	//eval () first
	while( iniBuffer.find( "(" ) != std::string::npos )
	{
		temp = iniBuffer.substr( iniBuffer.find( "(" ) + 1, iniBuffer.find( ")" ) - iniBuffer.find( "(" ) - 1  );
		_itoa_s( eval( temp ), iniBuf, sizeof( iniBuf ), 10 );
		iniBuffer.replace( iniBuffer.find( "(" ), iniBuffer.find( ")" ) - iniBuffer.find( "(" ) + 1, iniBuf );
	}

	//eval *,/,+,-
	for( int i = 0; i < 4; i++ )
	{	
		while( iniBuffer.find( operations[ i ] ) != std::string::npos )
		{
			temp = iniBuffer.substr( 0, iniBuffer.find( operations[ i ] ) );
			temp2 = iniBuffer.substr( iniBuffer.find( operations[ i ] ) );
			if( temp.find_last_of( "+-*/" ) != std::string::npos )
				temp = temp.substr( temp.find_last_of( "+-*/" ) );
			if( temp2.substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				temp2 = temp2.substr( 0, temp2.substr( 1 ).find_first_of( "+-*/" ) + 1 );

			_itoa_s( eval( temp.append( temp2 ) ), iniBuf, sizeof( iniBuf ), 10 );

			if( iniBuffer.substr( iniBuffer.find( operations[ i ] ) ).substr( 1 ).find_first_of( "+-*/" ) != std::string::npos )
				iniBuffer.replace( 0, iniBuffer.substr( iniBuffer.find( operations[ i ] ) ).substr( 1 ).find_first_of( "+-*/" ) 
					+ iniBuffer.find( operations[ i ] ) + 1, iniBuf );
			else
				iniBuffer.replace( iniBuffer.substr( 0, iniBuffer.find( operations[ i ] ) ).find_last_of( "+-*/" ) + 1, 
					iniBuffer.length( ) - iniBuffer.substr( 0, iniBuffer.find( operations[ i ] ) ).find_last_of( "+-*/" ), iniBuf );
		}
	}

	return atoi( iniBuffer.c_str( ) );
}
int SACodec::eval( std::string buffer )
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
int SACodec::readCTLElements( HANDLE originalFile, const char* key, const char* sig )
{
	int count = 0, currentBlock = 0, idLoc = 0;
	DWORD bytesRead = 0;
	bool found = false;
	char trueKey[ 256 ] = { 0 }, buffer[ MAX_READ_WRITE_SIZE ] = { 0 };;

	trueKey[ 0 ] = '!';
	strcat_s( trueKey, sizeof( trueKey ), key );
	strcat_s( trueKey, sizeof( trueKey ), "!" );

	SetFilePointer( originalFile, 0, NULL, FILE_BEGIN );
	bytesRead = MAX_READ_WRITE_SIZE;
	while( bytesRead == MAX_READ_WRITE_SIZE && found == false )
	{
		if( ReadFile( originalFile, buffer, MAX_READ_WRITE_SIZE, &bytesRead, NULL ) )
		{
			for( int i = 0; i < ( signed ) ( bytesRead - strlen( trueKey ) ); i++ )
			{
				if( buffer[ i ] == '!' ) 
				{
					for( int j = 0; j < ( signed ) ( strlen( trueKey ) ); j++ )
					{
						if( buffer[ i + j ] == trueKey[ j ] )
							count++;
					}
					if( count == strlen( trueKey ) )
					{
						idLoc = currentBlock + i;
						found = true;
						break;
					}
					else
						count = 0;
				}
			}
			currentBlock += bytesRead;
		}
	}

	if( found == false )
	{
		std::cout << "Could not find identifier" << std::endl;
		return 1;
	}

	count = 0;

	//read spacing between CTL elements from there + keylength + 1 to first !
	char tempSpacingBuffer[ 256 ] = { 0 };
	int spacing = 0;

	SetFilePointer( originalFile, ( idLoc + strlen( trueKey ) + 1 ), NULL, FILE_BEGIN );
	while( ReadFile( originalFile, buffer, 1, &bytesRead, NULL ) )
	{
		if( buffer[ 0 ] == '!' )
			break;
		else
		{
			tempSpacingBuffer[ count ] = buffer[ 0 ];
			count++;
		}
	}

	spacing = atoi( tempSpacingBuffer );

	//go through and collect CTL elements
	CTL controlSig = { 0 };
	DWORD fpPos[ 4 ][ 2 ] = { 0 }, sigFpPos = 0;

	for( int i = 1; i < 5; i++ )
	{
		count = 0;
		memset( tempSpacingBuffer, 0, 256 );

		fpPos[ i - 1 ][ 0 ] = SetFilePointer( originalFile, spacing * i, NULL, FILE_BEGIN );
		while( ReadFile( originalFile, buffer, 1, &bytesRead, NULL ) )
		{
			if( buffer[ 0 ] == '!' )
				break;
			else
			{
				tempSpacingBuffer[ count ] = buffer[ 0 ];
				count++;
			}
		}
		fpPos[ i - 1 ][ 1 ] = count;

		switch( i )
		{
		case 1:
			controlSig.fileSize = atoi( tempSpacingBuffer );
			break;
		case 2:
			controlSig.sigSize = atoi( tempSpacingBuffer );
			break;
		case 3:
			controlSig.sigSpacing = atoi( tempSpacingBuffer );
			break;
		case 4:
			controlSig.border = atoi( tempSpacingBuffer );
		}
	}

	if( controlSig.fileSize != GetFileSize( originalFile, NULL ) )
	{
		std::cout << "File sizes do not match" << std::endl;
		return 1;
	}

	//go through and read sig, see if it matches provided sig
	count = 0;
	for( int i = 1; i < (signed) ( controlSig.sigSize + 1 ); i++ )
	{
		sigFpPos = SetFilePointer( originalFile, controlSig.sigSpacing * i, NULL, FILE_BEGIN );
		for( int j = 0; j < 4; j++ )
		{
			if( sigFpPos > fpPos[ j ][ 0 ] && sigFpPos < fpPos[ j ][ 0 ] + fpPos[ j ][ 1 ] )
			{
				sigFpPos = SetFilePointer( originalFile, fpPos[ j ][ 0 ] + fpPos[ j ][ 1 ], NULL, FILE_BEGIN );
				break;
			}
		}
		ReadFile( originalFile, buffer, 1, &bytesRead, NULL );
		controlSig.sig[ count ] = buffer[ 0 ];
		count++;
	}

	if( strcmp( controlSig.sig, sig ) )
	{
		std::cout << "Signatures do not match" << std::endl;
		return 1;
	}

	return 0;
}
int SACodec::compareCrcs( HANDLE originalFile, const char* filePath )
{
	HANDLE crcFile = NULL;
	std::string fileName;
	char buffer[ MAX_READ_WRITE_SIZE ] = { 0 }, tempBuffer[ MAX_PATH ] = { 0 };
	DWORD bytesRead = 0;

	if( originalFile == INVALID_HANDLE_VALUE )
	{
		std::cout << "Could not find file to decode" << std::endl;
		return 1;
	}
	
	//verify the original crc and the current crc match
	fileName = filePath;
	fileName = fileName.substr( 0, fileName.find_last_of( "." ) );

	crcFile = CreateFile( reinterpret_cast< LPCSTR > ( fileName.c_str( ) ), GENERIC_READ, 0, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	
	if( crcFile == INVALID_HANDLE_VALUE )
	{
		std::cout << "Could not find crc file" << std::endl;
		return 1;
	}

	SetFilePointer( crcFile, 0, NULL, FILE_BEGIN );
	ReadFile( crcFile, buffer, sizeof( buffer ), &bytesRead, NULL );

	SetFilePointer( originalFile, 0, NULL, FILE_BEGIN );
	_itoa_s( genCRC32( originalFile ), tempBuffer, sizeof( tempBuffer ), 16 );

	if( strcmp( buffer, tempBuffer ) )
	{
		std::cout << "Crc's don't match" << std::endl;
		return 1;
	}

	CloseHandle( crcFile );


	return 0;
}