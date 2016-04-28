#!/usr/bin/php
<?PHP

require_once ( './shared.php' ) ;

$petscan = 'https://petscan.wmflabs.org' ;

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

function compareArrays ( $name , $a1 , $a2 ) {
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
		print "$name\tPASSED\n" ;
		return 1 ;
	}
	
	print "$name\tFAILED\n" ;
	foreach ( $only1 AS $a ) print "* Only in list 1: $a\n" ;
	foreach ( $only2 AS $a ) print "* Only in list 2: $a\n" ;
	return 0 ;
}



// TESTS

function testCategorySubset () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , 'Instrumental albums' , 2 , 0 , true ) ;
	$pages2 = getPagesInCategory ( $db , 'Free jazz albums' , 2 , 0 , true ) ;
	$result = getSubset ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 19833 ) ;
	return compareArrays ( "CategorySubset" , $result , $psd ) ;
}

function testPagePile () {
	$db = openDBwiki ( 'enwiki' ) ;
	$cat = getPagesInCategory ( $db , '1995 Formula One races' , 1 , 0 , true ) ;
	$pages = getPagePile ( 2939 ) ;
	$result = getSubset ( $cat , $pages ) ;
	$psd = getFromPSID ( 18588 ) ;
	return compareArrays ( "PagePile" , $result , $psd ) ;
}

function testSPARQL () {
	$sparql = 'SELECT ?item WHERE { ?item wdt:P180 wd:Q148706 }' ;
	$items_sparql = getSPARQLitems ( $sparql , 'item' ) ;
	$items_pagepile = getPagePile ( 2941 ) ;
	$result = getSubset ( $items_sparql , $items_pagepile ) ;
	$psd = getFromPSID ( 18607 ) ;
	return compareArrays ( "SPARQL" , $result , $psd ) ;
}

function testCategoryUnion () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , 'Sport in Varna' , 1 , 0 , true ) ;
	$pages2 = getPagesInCategory ( $db , '1396 deaths' , 1 , 0 , true ) ;
	$result = getUnion ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 18635 ) ;
	return compareArrays ( "CategoryUnion" , $result , $psd ) ;
}

function testNegativeCategories () {
	$db = openDBwiki ( 'enwiki' ) ;
	$pages1 = getPagesInCategory ( $db , 'Instrumental albums' , 0 , 0 , true ) ;
	$pages2 = getPagesInCategory ( $db , 'Free jazz albums' , 0 , 0 , true ) ;
	$result = getRemove2 ( $pages1 , $pages2 ) ;
	$psd = getFromPSID ( 19820 ) ;
	return compareArrays ( "NegativeCategories" , $result , $psd ) ;
	
}

// TEST CALLS

$tests = array ( 'PagePile' , 'SPARQL' , 'CategoryUnion' , 'NegativeCategories' , 'CategorySubset' ) ;

$total = 0 ;
$passed = 0 ;
foreach ( $tests AS $t ) {
	$fn = 'test' . $t ;
	$total++ ;
	$passed += $fn() ;
}
$failed = $total - $passed ;
print "$total tests; $passed passed, $failed failed\n" ;

?>