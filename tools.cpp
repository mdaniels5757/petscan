#include "main.h"
#include <fstream>
#include <streambuf>
#include <regex>

// trim from start
string ltrim(string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
string rtrim(string s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
string trim(string s) {
        return ltrim(rtrim(s));
}


//________________________________________________________________________________________________________________________

// Zero-pad
string pad ( string s , int num , char c ) {
	while ( s.length() < num ) s = c + s ;
	return s ;
}

//________________________________________________________________________________________________________________________

string getWikiServer ( string wiki ) {
	if ( wiki == "wikidatawiki" ) return "www.wikidata.org" ;
	if ( wiki == "commonswiki" ) return "commons.wikimedia.org" ;
	if ( wiki == "metawiki" ) return "commons.wikimedia.org" ;
	if ( wiki == "mediawikiwiki" ) return "www.mediawiki.org" ;
	
	for ( size_t a = 0 ; a+3 < wiki.length() ; a++ ) {
		if ( wiki[a]=='w' && wiki[a+1]=='i' && wiki[a+2]=='k' ) {
			string l = wiki.substr(0,a) ;
			string p = wiki.substr(a) ;
			if ( p == "wiki" ) p = "wikipedia" ;
			else if ( p.length()>4 && p.substr(p.length()-4,4) == "wiki" ) p = p.substr(0,p.length()-4) ;
//			else regex_replace ( p , regex("wiki$") , string("") ) ;
			return l+"."+p+".org" ;
		}
	}
	return "NO_SERVER_FOUND" ; // Say what?
}

//________________________________________________________________________________________________________________________

char *loadFileFromDisk ( string filename ) {
//printf ( "BEGIN %s\n" , filename.c_str() ) ;
	char * buffer;
	size_t result;
  	FILE *pFile = fopen ( filename.c_str() , "rb" );
	if (pFile==NULL) { if(DEBUG_OUTPUT) cout << "Cannot open file " << filename << endl ; return NULL ; }
	
	fseek (pFile , 0 , SEEK_END);
	long lSize = ftell (pFile);
	rewind (pFile);
	
	buffer = (char*) malloc (sizeof(char)*(lSize+1));
	if (buffer == NULL) { if(DEBUG_OUTPUT) cout << "Memory error while reading file " << filename << endl ; return NULL ; }

	result = fread (buffer,1,lSize,pFile);
	if (result != lSize) { if(DEBUG_OUTPUT) cout << "Reading error for file " << filename << endl ; return NULL ; }
	fclose (pFile);
	buffer[lSize] = 0 ;
	
//printf ( "END %s\n" , filename.c_str() ) ;
	return buffer ;
}


std::mutex g_file_cache_mutex;
map <string,string> file_cache ;

string loadAndCacheFileFromDisk ( string filename ) {
	std::lock_guard<std::mutex> guard(g_file_cache_mutex);
	if ( file_cache.find(filename) != file_cache.end() ) return file_cache[filename] ;
	ifstream ifs(filename.c_str());
	string content ( (std::istreambuf_iterator<char>(ifs) ), (std::istreambuf_iterator<char>() ) ) ;
	if ( root_platform && root_platform->config.find("testing") == root_platform->config.end() ) { // Use caching, unless "testing" set in config
		file_cache[filename] = content ;
	}
	return content ;
}


//________________________________________________________________________________________________________________________


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

//________________________________________________________________________________________________________________________

void stringReplace(std::string& str, string oldStr, string newStr){
  size_t pos = 0;
  while((pos = str.find(oldStr, pos)) != std::string::npos){
     str.replace(pos, oldStr.length(), newStr);
     pos += newStr.length();
  }
}

//________________________________________________________________________________________________________________________

map <string,string> url_json_cache ;

bool loadJSONfromURL ( string url , json &j , bool use_cache ) {
	if ( use_cache && url_json_cache.find(url) != url_json_cache.end() ) {
		string text = url_json_cache[url] ;
		j = json::parse ( text.c_str() ) ;
		return true ;
	}
	CURL *curl;
	curl = curl_easy_init();
	if ( !curl ) return false ;

	struct CURLMemoryStruct chunk;
	chunk.memory = (char*) malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */ 

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect; paranoia
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "petscan-agent/1.0"); // fake agent
	
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) return false ;
	if ( chunk.size == 0 || !chunk.memory ) return false ;
	
	char *text = chunk.memory ;
	curl_easy_cleanup(curl);

	if ( *text != '{' ) {
		free ( text ) ;
		return false ;
	}
	
	j = json::parse ( text ) ;
	
	if ( use_cache ) {
		std::lock_guard<std::mutex> guard(g_file_cache_mutex);
		url_json_cache[url] = string ( text ) ;
	}
	
	free ( text ) ;
	return true ;
}

//________________________________________________________________________________________________________________________

string space2_ ( string s ) {
	string ret = s ;
	std::replace ( ret.begin(), ret.end(), ' ', '_') ;
	return ret ;
}

string _2space ( string s ) {
	string ret = s ;
	std::replace ( ret.begin(), ret.end(), '_', ' ') ;
	return ret ;
}

string ui2s ( uint32_t i ) {
	char tmp[20] ;
	sprintf ( tmp , "%d" , i ) ;
	return string(tmp) ;
}

//________________________________________________________________________________________________________________________


//string escapeURLcomponent ( string s ) {
const std::string urlencode( const std::string& s ) {
	CURL *curl;
	curl = curl_easy_init();
	if ( !curl ) return "" ;
	char *encoded_query = curl_easy_escape ( curl , s.c_str() , 0 ) ;
	string ret ( encoded_query ) ;
	curl_free(encoded_query) ;
	curl_easy_cleanup(curl);
	return ret ;
}

string escapeURLcomponent ( string s ) {
	return urlencode ( s ) ;
}
//________________________________________________________________________________________________________________________

double time_diff(struct timeval x , struct timeval y) {
    double x_ms , y_ms , diff;
     
    x_ms = (double)x.tv_sec*1000000 + (double)x.tv_usec;
    y_ms = (double)y.tv_sec*1000000 + (double)y.tv_usec;
     
    diff = (double)y_ms - (double)x_ms;
     
    return diff;
}
