#include "main.h"

//________________________________________________________________________________________________________________________

bool TPlatform::readConfigFile ( string filename ) {
	char *buffer = loadFileFromDisk ( filename ) ;
	MyJSON j ( buffer ) ;
	free (buffer);
	
	for ( auto i = j.o.begin() ; i != j.o.end() ; i++ ) {
		if ( i->first == "" ) continue ;
		config[i->first] = i->second.s ;
	}

	return true ;
}

char *loadFileFromDisk ( string filename ) {
	char * buffer;
	size_t result;
  	FILE *pFile = fopen ( filename.c_str() , "rb" );
	if (pFile==NULL) { cerr << "Cannot open file " << filename << endl ; exit ( 0 ) ; }
	
	fseek (pFile , 0 , SEEK_END);
	long lSize = ftell (pFile);
	rewind (pFile);
	
	buffer = (char*) malloc (sizeof(char)*(lSize+1));
	if (buffer == NULL) { cerr << "Memory error while reading file " << filename << endl ; exit ( 0 ) ; }

	result = fread (buffer,1,lSize,pFile);
	if (result != lSize) { cerr << "Reading error for file " << filename << endl ; exit ( 0 ) ; }
	fclose (pFile);
	buffer[lSize] = 0 ;
	
	return buffer ;
}



void split ( const string &input , vector <string> &v , char delim , uint32_t max ) {
	v.clear() ;
	istringstream f(input);
	string s;    
	while (getline(f, s, delim)) {
		if ( max > 0 && v.size() == max ) {
			v[max-1] += delim ;
			v[max-1] += s ;
		} else v.push_back(s);
	}
}


//________________________________________________________________________________________________________________________

// Taken from http://dlib.net/dlib/server/server_http.cpp.html
// under boost license

inline unsigned char to_hex( unsigned char x )  
{
	return x + (x > 9 ? ('A'-10) : '0');
}

const std::string urlencode( const std::string& s )  
{
	std::ostringstream os;

	for ( std::string::const_iterator ci = s.begin(); ci != s.end(); ++ci )
	{
		if ( (*ci >= 'a' && *ci <= 'z') ||
			 (*ci >= 'A' && *ci <= 'Z') ||
			 (*ci >= '0' && *ci <= '9') )
		{ // allowed
			os << *ci;
		}
		else if ( *ci == ' ')
		{
			os << '+';
		}
		else
		{
			os << '%' << to_hex(*ci >> 4) << to_hex(*ci % 16);
		}
	}

	return os.str();
}

inline unsigned char from_hex (
	unsigned char ch
) 
{
	if (ch <= '9' && ch >= '0')
		ch -= '0';
	else if (ch <= 'f' && ch >= 'a')
		ch -= 'a' - 10;
	else if (ch <= 'F' && ch >= 'A')
		ch -= 'A' - 10;
	else 
		ch = 0;
	return ch;
}

const std::string urldecode (
	const std::string& str
) 
{
	using namespace std;
	string result;
	string::size_type i;
	for (i = 0; i < str.size(); ++i)
	{
		if (str[i] == '+')
		{
			result += ' ';
		}
		else if (str[i] == '%' && str.size() > i+2)
		{
			const unsigned char ch1 = from_hex(str[i+1]);
			const unsigned char ch2 = from_hex(str[i+2]);
			const unsigned char ch = (ch1 << 4) | ch2;
			result += ch;
			i += 2;
		}
		else
		{
			result += str[i];
		}
	}
	return result;
}