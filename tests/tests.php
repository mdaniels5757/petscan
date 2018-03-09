#!/usr/bin/php
<?PHP

require_once ( './shared.php' ) ;

$petscan = 'https://petscan-dev.wmflabs.org' ;

function getFromPSID ( $psid ) {
	global $petscan ;
	$url = "$petscan/?psid=$psid&format=json&sparse=1" ;
	$j = json_decode ( file_get_contents ( $url ) ) ;
	$star = '*' ;
	$ret = $j->$star ;
	$ret = $ret[0]->a->$star ;
	return $ret ;
}

function getPagePile ( $pile ) {
	$url = "https://tools.wmflabs.org/pagepile/api.php?id=$pile&action=get_data&format=json&doit" ;
	$j = json_decode ( file_get_contents ( $url ) ) ;
	return $j->pages ;
}

function getSubset ( $a1 , $a2 ) {
	$ret = array() ;
	foreach ( $a1 AS $a ) {
		if ( in_array ( $a , $a2 ) ) $ret[] = $a ;
	}
	return $ret ;
}

function getUnion ( $pages1 , $pages2 ) {
	$ret = array() ;
	foreach ( $pages1 AS $p ) $ret[$p] = $p ;
	foreach ( $pages2 AS $p ) $ret[$p] = $p ;
	return $ret ;
}

function getRemove2 ( $pages1 , $pages2 ) {
	$ret = array () ;
	foreach ( $pages1 AS $p ) {
		if ( !in_array ( $p , $pages2 ) ) $ret[] = $p ;
	}
	return $ret ;
}

function compareArrays ( $a1 , $a2 ) {
	global $test_name ;

	if ( count($a1) == 0 || count($a2) == 0 ) {
		print "ZERO\t$test_name\t" . count($a1) . '/' . count($a2) . "\n" ;
		return 0 ;
	}
	

	$only1 = array() ;
	$only2 = array() ;
	$both = 0 ;
	
	foreach ( $a1 AS $a ) {
		if ( !in_array ( $a , $a2 ) ) {
			$only1[] = $a ;
		} else $both++ ;
	}

	foreach ( $a2 AS $a ) {
		if ( !in_array ( $a , $a1 ) ) {
			$only2[] = $a ;
		}
	}
	
	if ( count($only1) == 0 and count($only2) == 0 and count($a1) == $both ) {
		print "PASSED\t$test_name\n" ;
		return 1 ;
	}
	
	print "FAILED\t$test_name\n" ;
	foreach ( $only1 AS $a ) print "* Only in list 1: $a\n" ;
	foreach ( $only2 AS $a ) print "* Only in list 2: $a\n" ;
	return 0 ;
}



// TESTS

function testCategorySubset () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , 'Instrumental albums' , 2 , 0 , true ) ;
	$pages2 = getPagesInCategory ( $db , 'Free jazz albums' , 2 , 0 , true ) ;
	$results = getSubset ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 19833 ) ;
	return compareArrays ( $results , $psd ) ;
}

function testPagePile () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , '1995 Formula One races' , 1 , 0 , true ) ;
	$pages2 = getPagePile ( 2939 ) ;
	$results = getSubset ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 18588 ) ;
	return compareArrays ( $results , $psd ) ;
}

function testSPARQL () {
	$sparql = 'SELECT ?item WHERE { ?item wdt:P180 wd:Q148706 }' ;
	$pages1 = getSPARQLitems ( $sparql , 'item' ) ;
	$pages2 = getPagePile ( 2941 ) ;
	$results = getSubset ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 18607 ) ;
	return compareArrays ( $results , $psd ) ;
}

function testCategoryUnion () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , 'Sport in Varna' , 1 , 0 , true ) ;
	$pages2 = getPagesInCategory ( $db , '1396 deaths' , 1 , 0 , true ) ;
	$results = getUnion ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 18635 ) ;
	return compareArrays ( $results , $psd ) ;
}

function testNegativeCategories () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , 'Instrumental albums' , 0 , 0 , true ) ;
	$pages2 = getPagesInCategory ( $db , 'Free jazz albums' , 0 , 0 , true ) ;
	$results = getRemove2 ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 19820 ) ;
	return compareArrays ( $results , $psd ) ;
	
}

function testWikidataSitelink () {
	$db = openDBwiki ( 'wikidatawiki' ) ;
	$sql = "SELECT page_title AS q FROM wb_items_per_site,pagelinks,page WHERE pl_from=page_id AND pl_namespace=120 AND pl_title='P1415' AND page_namespace=0 AND page_title=concat('Q',ips_item_id) AND ips_site_id='aywiki'" ;
	if(!$result = $db->query($sql)) die('There was an error running the query [' . $db->error . ']');
	$results = array() ;
	while($o = $result->fetch_object()) $results[] = $o->q ;
	$psd = getFromPSID ( 19977 ) ;
	return compareArrays ( $results , $psd ) ;
}

function testWikidataUses () {
	$db = openDBwiki ( 'wikidatawiki' ) ;

	$sql = "SELECT page_title AS q FROM wb_items_per_site,pagelinks,page WHERE pl_from=page_id AND pl_namespace=120 AND pl_title='P1415' AND page_namespace=0 AND page_title=concat('Q',ips_item_id) AND ips_site_id='aywiki'" ;
	if(!$result = $db->query($sql)) die('There was an error running the query [' . $db->error . ']');
	$pages1 = array() ;
	while($o = $result->fetch_object()) $pages1[] = $o->q ;

	$sql = "SELECT page_title AS q FROM wb_items_per_site,pagelinks,page WHERE pl_from=page_id AND pl_namespace=0 AND pl_title='Q1860' AND page_namespace=0 AND page_title=concat('Q',ips_item_id) AND ips_site_id='aywiki'" ;
	if(!$result = $db->query($sql)) die('There was an error running the query [' . $db->error . ']');
	$pages2 = array() ;
	while($o = $result->fetch_object()) $pages2[] = $o->q ;
	
	$results = getUnion ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 20004 ) ;
	return compareArrays ( $results , $psd ) ;
}


// TEST CALLS

$tests = array ( 'CategoryUnion' , 'NegativeCategories' , 'CategorySubset' , 'WikidataSitelink' , 'WikidataUses' ) ; // 'PagePile' and 'SPARQL'  deactivated, piles are missing...
//$tests = array ( array_pop ( $tests ) ) ; // Run last test only; useful for testing a new test

$total = 0 ;
$passed = 0 ;
foreach ( $tests AS $test_name ) {
	$fn = 'test' . $test_name ;
	$total++ ;
	$passed += $fn() ;
}
$failed = $total - $passed ;
print "$total tests; $passed passed, $failed failed\n" ;

?>