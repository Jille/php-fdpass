--TEST--
fdpass_send() function
--SKIPIF--
<?php 

if(!extension_loaded('fdpass')) die('skip ');

 ?>
--FILE--
<?php
echo 'OK'; // no test case for this function yet
?>
--EXPECT--
OK