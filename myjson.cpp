#include "myjson.h"

#define SKIP_BLANK(__c) {while ( *__c==32||*__c==9||*__c==10||*__c==13 ) (__c)++ ;}


char *MyJSON::parse ( char *t , int depth ) {
	SKIP_BLANK(t) ;
	if ( *t == 0 ) { cout << "!!!0\n" ; return NULL ;} 

	char *c = t ;

	if ( *t == '{' ) {
		isa = MJ_OBJECT ;
		c++ ;
		SKIP_BLANK(c) ;
		while ( *c && *c != '}' ) {
			MyJSON k , v ;
			c = k.parse ( c , depth+1 ) ;
//			cout << "OBJECT KEY: " << k.s << endl ;
			SKIP_BLANK(c) ;
			if ( *c == ':' ) c++ ;
			else { cout << "!!!1\n" << c ; return NULL ;}  // FIXME
			SKIP_BLANK(c) ;
			o[k.s] = MyJSON(NULL) ;
			c = o[k.s].parse ( c , depth+1 ) ;
//			o[k.s] = v ; // FIXME numerical keys etc.
			SKIP_BLANK(c) ;
			if ( *c == ',' ) c++ ;
			SKIP_BLANK(c) ;
		}
		if ( *c == '}' ) c++ ;
		return c ;
	}

	if ( *t == '[' ) {
		isa = MJ_ARRAY ;
//		a.reserve ( 50 ) ;
		c++ ;
		while ( *c && *c != ']' ) {
			a.push_back ( MyJSON() ) ;
			c = a[a.size()-1].parse ( c , depth+1 ) ;
			if ( *c == ',' ) c++ ;
		}
		if ( *c == ']' ) c++ ;
		return c ;
	}
	
	if ( *t == '"' ) {
		isa = MJ_STRING ;
		for ( c++ ; *c && *c != '"' ; c++ ) {
			s += *c ;
			if ( *c == '\\' ) {
				c++ ;
				s += *c ;
			}
		}
		if ( *c == '"' ) c++ ;
		s = unescapeString ( s ) ;
		return c ;
	}
	
	// null
	if ( *t == 'n' && *(t+1) == 'u' && *(t+1) == 'u' && *(t+2) == 'l' && *(t+3) == 'l' ) {
		isa = MJ_NULL ;
		c += 4 ;
		return c ;
	}
	
	// Number
	isa = MJ_FLOAT ;
	string z ;
	if ( *c == '+' ) c++ ;
	for ( ; *c && *c != ',' && *c != ']' && *c != '}' ; c++ ) z += *c ;
	i = atol ( z.c_str() ) ;
	f = atof ( z.c_str() ) ;
	return c ;
}

void MyJSON::print ( ostream &out ) {
	bool first = true ;
	switch ( isa ) {
		case MJ_STRING : out << "\"" << escapeString(s) << "\"" ; break ;
		case MJ_INT : out << i  ; break ;
		case MJ_FLOAT : out << f  ; break ;
		case MJ_ARRAY :	out << "[" ;
			for ( uint32_t x = 0 ; x < a.size() ; x++ ) {
				if ( !first ) out << "," ;
				a[x].print(out) ;
				first = false ;
			}
			out << "]" ;
			break ;
		case MJ_OBJECT : out << "{" ;
			for ( map <string,MyJSON>::iterator x = o.begin() ; x != o.end() ; x++ ) {
				if ( !first ) out << "," ;
				out << "\"" << x->first << "\":" ;
				x->second.print(out) ;
				first = false ;
			}
			out << "}" ;
			break ;
		case MJ_NULL : out << "null" << endl ; break ;
		case MJ_UNKNOWN : cerr << "UNKNOWN" << endl ; exit(0); break ;
	}
}



string MyJSON::escapeString ( const string &s ) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f')) {
            o << "\\u"
              << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
        } else {
            o << *c;
        }
    }
    return o.str();
}

string MyJSON::unescapeString ( const string &input ) {
	string result;
	result.reserve(static_cast<size_t>(input.length()));
	const unsigned char *m_limit = (const unsigned char *) (input.c_str() + input.length()) ;

	// iterate the result between the quotes
	for (const unsigned char * i = (const unsigned char*) input.c_str(); *i ; ++i) {
		// process escaped characters
		if (*i == '\\') {
			// read next character
			++i;
			switch (*i) { // the default escapes
				case 't': { result += "\t"; break; }
				case 'b': { result += "\b"; break; }
				case 'f': { result += "\f"; break; }
				case 'n': { result += "\n"; break; }
				case 'r': { result += "\r"; break; }
				case '\\': { result += "\\"; break; }
				case '/': { result += "/"; break; }
				case '"': { result += "\""; break; }

				// unicode
				case 'u': { // get code xxxx from uxxxx
					auto codepoint = std::strtoul(std::string(reinterpret_cast<typename string::const_pointer>(i + 1), 4).c_str(), nullptr, 16);
					// check if codepoint is a high surrogate
					if (codepoint >= 0xD800 and codepoint <= 0xDBFF) { // make sure there is a subsequent unicode
						if ((i + 6 >= m_limit) or * (i + 5) != '\\' or * (i + 6) != 'u') { throw std::invalid_argument("missing low surrogate"); }
						// get code yyyy from uxxxx\uyyyy
						auto codepoint2 = std::strtoul(std::string(reinterpret_cast<typename string::const_pointer>
						(i + 7), 4).c_str(), nullptr, 16);
						result += to_unicode(codepoint, codepoint2);
						// skip the next 10 characters (xxxx\uyyyy)
						i += 10;
					}
					else { // add unicode character(s)
						result += to_unicode(codepoint);
						i += 4; // skip the next four characters (xxxx)
					}
					break;
				}
			}
		} else { // all other characters are just copied to the end of the string
			result.append(1, static_cast<typename string::value_type>(*i));
		}
	}
	return result;
}

string MyJSON::to_unicode(const std::size_t codepoint1, const std::size_t codepoint2) {
	string result;
	std::size_t codepoint = codepoint1;

	if (codepoint1 >= 0xD800 and codepoint1 <= 0xDBFF) { // check if codepoint1 is a high surrogate
		if (codepoint2 >= 0xDC00 and codepoint2 <= 0xDFFF) { // check if codepoint2 is a low surrogate
			codepoint = (codepoint1 << 10) + codepoint2 - 0x35FDC00;
		} else {
			throw std::invalid_argument("missing or wrong low surrogate");
		}
	}

	if (codepoint < 0x80) result.append(1, static_cast<typename string::value_type>(codepoint));
	else if (codepoint <= 0x7ff) {
		result.append(1, static_cast<typename string::value_type>(0xC0 | ((codepoint >> 6) & 0x1F)));
		result.append(1, static_cast<typename string::value_type>(0x80 | (codepoint & 0x3F)));
	} else if (codepoint <= 0xffff) {
		result.append(1, static_cast<typename string::value_type>(0xE0 | ((codepoint >> 12) & 0x0F)));
		result.append(1, static_cast<typename string::value_type>(0x80 | ((codepoint >> 6) & 0x3F)));
		result.append(1, static_cast<typename string::value_type>(0x80 | (codepoint & 0x3F)));
	} else if (codepoint <= 0x10ffff) {
		result.append(1, static_cast<typename string::value_type>(0xF0 | ((codepoint >> 18) & 0x07)));
		result.append(1, static_cast<typename string::value_type>(0x80 | ((codepoint >> 12) & 0x3F)));
		result.append(1, static_cast<typename string::value_type>(0x80 | ((codepoint >> 6) & 0x3F)));
		result.append(1, static_cast<typename string::value_type>(0x80 | (codepoint & 0x3F)));
	} else {
		throw std::out_of_range("code points above 0x10FFFF are invalid");
	}
	return result;
}
