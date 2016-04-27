<?PHP

error_reporting(E_ERROR|E_CORE_ERROR|E_ALL|E_COMPILE_ERROR);

ini_set('user_agent','PetScan testing tool'); # Fake user agent

function myurlencode ( $t ) {
	$t = str_replace ( " " , "_" , $t ) ;
	$t = urlencode ( $t ) ;
	return $t ;
}

function getWebserverForWiki ( $wiki ) {
	$wiki = preg_replace ( '/_p$/' , '' , $wiki ) ; // Paranoia
	if ( $wiki == 'commonswiki' ) return "commons.wikimedia.org" ;
	if ( $wiki == 'wikidatawiki' ) return "www.wikidata.org" ;
	if ( $wiki == 'specieswiki' ) return "species.wikimedia.org" ;
	$wiki = preg_replace ( '/_/' , '-' , $wiki ) ;
	if ( preg_match ( '/^(.+)wiki$/' , $wiki , $m ) ) return $m[1].".wikipedia.org" ;
	if ( preg_match ( '/^(.+)(wik.+)$/' , $wiki , $m ) ) return $m[1].".".$m[2].".org" ;
	return '' ;
}

function escape_attribute ( $s ) {
	return preg_replace ( "/'/" , '&quot;' , $s ) ;
}

function getDBpassword () {
	global $mysql_user , $mysql_password , $tool_user_name ;
	$j = json_decode ( file_get_contents ( '/home/magnus/petscan/config.json' ) ) ;
	$mysql_user = $j->user ;
	$mysql_password = $j->password ;
}

function getDBname ( $language , $project ) {
	$ret = $language ;
	if ( $language == 'commons' ) $ret = 'commonswiki_p' ;
	elseif ( $language == 'wikidata' || $project == 'wikidata' ) $ret = 'wikidatawiki_p' ;
	elseif ( $language == 'mediawiki' || $project == 'mediawiki' ) $ret = 'mediawikiwiki_p' ;
	elseif ( $language == 'meta' && $project == 'wikimedia' ) $ret = 'metawiki_p' ;
	elseif ( $project == 'wikipedia' ) $ret .= 'wiki_p' ;
	elseif ( $project == 'wikisource' ) $ret .= 'wikisource_p' ;
	elseif ( $project == 'wiktionary' ) $ret .= 'wiktionary_p' ;
	elseif ( $project == 'wikibooks' ) $ret .= 'wikibooks_p' ;
	elseif ( $project == 'wikinews' ) $ret .= 'wikinews_p' ;
	elseif ( $project == 'wikiversity' ) $ret .= 'wikiversity_p' ;
	elseif ( $project == 'wikivoyage' ) $ret .= 'wikivoyage_p' ;
	elseif ( $project == 'wikiquote' ) $ret .= 'wikiquote_p' ;
	elseif ( $project == 'wikispecies' ) $ret = 'specieswiki_p' ;
	elseif ( $language == 'meta' ) $ret .= 'metawiki_p' ;
	else if ( $project == 'wikimedia' ) $ret .= $language.$project."_p" ;
	else die ( "Cannot construct database name for $language.$project - aborting." ) ;
	return $ret ;
}

function openToolDB ( $dbname = '' , $server = '' , $force_user = '' ) {
	global $o , $mysql_user , $mysql_password ;
	getDBpassword() ;
	if ( $dbname == '' ) $dbname = '_main' ;
	else $dbname = "__$dbname" ;
	if ( $force_user == '' ) $dbname = $mysql_user.$dbname;
	else $dbname = $force_user.$dbname;
	if ( $server == '' ) $server = "tools-db" ;
	$db = new mysqli($server, $mysql_user, $mysql_password, $dbname);
	if($db->connect_errno > 0) {
		$o['msg'] = 'Unable to connect to database [' . $db->connect_error . ']';
		$o['status'] = 'ERROR' ;
		return false ;
	}
	return $db ;
}

function openDBwiki ( $wiki ) {
	preg_match ( '/^(.+)(wik.+)$/' , $wiki , $m ) ;
	if ( $m == null ) {
		print "Cannot parse $wiki\n" ;
		return ;
	}
	if ( $m[2] == 'wiki' ) $m[2] = 'wikipedia' ;
	return openDB ( $m[1] , $m[2] ) ;
}

function openDB ( $language , $project ) {
	global $db ;
	global $mysql_user , $mysql_password , $o , $common_db_cache , $use_db_cache ;
	
	$db_key = "$language.$project" ;
	if ( isset ( $common_db_cache[$db_key] ) ) return $common_db_cache[$db_key] ;
	
	getDBpassword() ;
	$dbname = getDBname ( $language , $project ) ;

	$server = substr( $dbname, 0, -2 ) . '.labsdb';
//$server = 'c3.labsdb' ; # TEMPORARY HACK TO BYPASS BROKEN labs1002 SERVER
	$db = new mysqli($server, $mysql_user, $mysql_password, $dbname);
	if($db->connect_errno > 0) {
		$o['msg'] = 'Unable to connect to database [' . $db->connect_error . ']';
		$o['status'] = 'ERROR' ;
		return false ;
	}
	if ( $use_db_cache ) $common_db_cache[$db_key] = $db ;
	return $db ;
}

function get_db_safe ( $s , $fixup = false ) {
	global $db ;
	if ( $fixup ) $s = str_replace ( ' ' , '_' , trim ( ucfirst ( $s ) ) ) ;
	if ( !isset($db) ) return addslashes ( str_replace ( ' ' , '_' , $s ) ) ;
	return $db->real_escape_string ( str_replace ( ' ' , '_' , $s ) ) ;
}

function make_db_safe ( &$s , $fixup = false ) {
	$s = get_db_safe ( $s , $fixup ) ;
}

function findSubcats ( $db , $root , &$subcats , $depth = -1 ) {
	global $testing ;
	$check = array() ;
	$c = array() ;
	foreach ( $root AS $r ) {
		if ( isset ( $subcats[$r] ) ) continue ;
		$subcats[$r] = get_db_safe ( $r ) ;
		$c[] = get_db_safe($r); //str_replace ( ' ' , '_' , $db->escape_string ( $r ) ) ;
	}
	if ( count ( $c ) == 0 ) return ;
	if ( $depth == 0 ) return ;
	$sql = "select distinct page_title from page,categorylinks where page_id=cl_from and cl_to IN ('" . implode ( "','" , $c ) . "') and cl_type='subcat'" ;
	if(!$result = $db->query($sql)) die('There was an error running the query [' . $db->error . ']');
	while($row = $result->fetch_assoc()){
		if ( isset ( $subcats[$row['page_title']] ) ) continue ;
		$check[] = $row['page_title'] ;
	}
	if ( count ( $check ) == 0 ) return ;
	findSubcats ( $db , $check , $subcats , $depth - 1 ) ;
}

function getPagesInCategory ( $db , $category , $depth = 0 , $namespace = 0 , $no_redirects = false ) {
	global $testing ;
	$ret = array() ;
	$cats = array() ;
	findSubcats ( $db , array($category) , $cats , $depth ) ;
	if ( $namespace == 14 ) return $cats ; // Faster, and includes root category

	$namespace *= 1 ;
	$sql = "SELECT DISTINCT page_title FROM page,categorylinks WHERE cl_from=page_id AND page_namespace=$namespace AND cl_to IN ('" . implode("','",$cats) . "')" ;
	if ( $no_redirects ) $sql .= " AND page_is_redirect=0" ;

	if(!$result = $db->query($sql)) die('There was an error running the query [' . $db->error . ']');
	while($o = $result->fetch_object()){
		$ret[$o->page_title] = $o->page_title ;
	}
	return $ret ;
}




function getSPARQLprefixes () {
	$sparql = '' ;
#	$sparql .= "PREFIX wdt: <http://www.wikidata.org/prop/direct/>\n" ;
#	$sparql .= "PREFIX wd: <http://www.wikidata.org/entity/>\n" ;
#	$sparql .= "PREFIX wikibase: <http://wikiba.se/ontology#>\n" ;
#	$sparql .= "PREFIX p: <http://www.wikidata.org/prop/>\n" ;
	$sparql .= "PREFIX v: <http://www.wikidata.org/prop/statement/>\n" ;
	$sparql .= "PREFIX q: <http://www.wikidata.org/prop/qualifier/>\n" ;
	$sparql .= "PREFIX ps: <http://www.wikidata.org/prop/statement/>\n" ;
	$sparql .= "PREFIX pq: <http://www.wikidata.org/prop/qualifier/>\n" ;
#	$sparql .= "PREFIX rdfs: <http://www.w3.org/2000/01/rdf-schema#>\n" ;
#	$sparql .= "PREFIX schema: <http://schema.org/>\n" ;
#	$sparql .= "PREFIX psv: <http://www.wikidata.org/prop/statement/value/>\n" ;
	return $sparql ;
}

function getSPARQL ( $cmd ) {
	$sparql = getSPARQLprefixes() ;
	$sparql .= $cmd ;

$ctx = stream_context_create(array('http'=>
    array(
        'timeout' => 1200,  //1200 Seconds is 20 Minutes
    )
));

#	$url = "https://query.wikidata.org/bigdata/namespace/wdq/sparql?format=json&query=" . urlencode($sparql) ;
	$url = "https://query.wikidata.org/sparql?format=json&query=" . urlencode($sparql) ;
	$fc = @file_get_contents ( $url , false , $ctx ) ;
	if ( $fc === false ) return ; // Nope
	return json_decode ( $fc ) ;
}


function getSPARQLitems ( $cmd , $varname = 'q' ) {
	$ret = array() ;
	$j = getSPARQL ( $cmd ) ;
	if ( !isset($j->results) or !isset($j->results->bindings) or count($j->results->bindings) == 0 ) return $ret ;
	foreach ( $j->results->bindings AS $v ) {
		$ret[] = preg_replace ( '/^.+\/Q/' , 'Q' , $v->$varname->value ) ;
	}
	return $ret ;
}


?>