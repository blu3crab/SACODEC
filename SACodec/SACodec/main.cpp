/*
 SACodec - 09JUN11
 author: Nathan
*/

#include <windows.h>
#include <iostream>
#include "SACodec.h"

int main( int argc, char* argv[ ] ) 
{
	int edcoder = 0;
	char *filename = NULL, *key = NULL, *sig = NULL;
	SACodec c;
	bool help = false;

	for( int i = 1; i < argc; i++ )
	{
		if( !strcmp( argv[ i ], "-e" ) )
			edcoder = 1;
		else if( !strcmp( argv[ i ], "-d" ) )
			edcoder = 2;
		else if( !strcmp( argv[ i ], "-f" ) )
		{
			filename = ( char* ) malloc( 256 );
			strcpy_s( filename, 256 , argv[ i + 1 ] );
			i++;
		}
		else if( !strcmp( argv[ i ], "-k" ) )
		{
			key = ( char* ) malloc( 256 );
			strcpy_s( key, 256, argv[ i + 1 ] );
			i++;
		}
		else if( !strcmp( argv[ i ], "-s" ) )
		{
			sig = ( char* ) malloc( 256 );
			strcpy_s( sig, 256, argv[ i + 1 ] );
			i++;
		}
		else if( !strcmp( argv[ i ], "-help" ) )
		{
			help = true;
			std::cout << "Usage: SACodec [-e | -d | -k KEY | -s SIGNATURE | -f FILENAME ]" << std::endl << std::endl
				<< "-e: Encode file \t -d: Decode file\t(Either -e or -d is required.)\n-k: Provide optional key for decoding\n-s: Provide optional signature for decoding"
				<< std::endl << "-f: Provide file name to encode/decode. Required." << std::endl << std::endl;
		}
		else
			std::cout << "Unknown switch passed. Please type -help for more details." << std::endl;
	}
	
	if( !help )
	{
		if( filename == NULL )
		{
			std::cout << "Please provide a filename with the -f switch" << std::endl;
			return 1;
		}

		if( edcoder == 0 )
		{
			std::cout << "Please pass either a -e or -d switch" << std::endl;
			return 1;
		}
		else if( edcoder == 1 )
		{
			if( c.EncodeFile( filename ) )
				std::cout << "Problem encoding the file" << std::endl;
			else
				std::cout << "---" << std::endl << "File " << filename << " encoded successfully" << std::endl;
		}
		else
		{
			if( c.DecodeFile( filename, key, sig ) )
				std::cout << "Problem decoding the file" << std::endl;
			else
				std::cout << "---" << std::endl << "File " << filename << " decoded successfully" << std::endl;
		}
	}

	free( filename );
	free( key );
	free( sig );

	filename = key = sig = NULL;

	return 0;
}