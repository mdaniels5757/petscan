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
	
	if ( *t == '"' ) { // NOTE: Strings will still be escaped and JSON-encoded, \uXXX etc.
		isa = MJ_STRING ;
		for ( c++ ; *c && *c != '"' ; c++ ) {
			s += *c ;
			if ( *c == '\\' ) {
				c++ ;
				s += *c ;
			}
		}
		if ( *c == '"' ) c++ ;
		return c ;
	}
	
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
		case MJ_STRING : out << "\"" << s << "\"" ; break ;
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