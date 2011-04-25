<?php
	@dl("fdpass.so");

	if(!function_exists('fdpass_send')) {
		print("fdpass_send not found. Extension not loaded?\n");
		exit(1);
	}

	$pair = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);

	$pid = pcntl_fork();
	switch($pid) {
		case -1:
			print("Fork failed\n");
			exit(1);
		case 0: // child
			fclose($pair[0]);
			sleep(1);
			$s = NULL;
			$r = fdpass_recv($pair[1], $s);
			var_dump($r, $s);
			# var_dump(fread($pair[1], 512));
			fwrite($s, "GET /\n");
			var_dump(fgets($s, 8192));
			echo "Later.\n";
			exit(0);
		default: // parent;
			fclose($pair[1]);
			$sock = stream_socket_client('tcp://10.0.0.6:1337');
			# fwrite($pair[0], "Hoi\n");
			$r = fdpass_send($pair[0], $sock, 'Hoi\n');
			var_dump($r);
			$status = NULL;
			var_dump(pcntl_wait($pid, $status), $status);
			exit(0);
	}
?>
