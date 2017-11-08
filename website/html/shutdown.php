<?php

if(isset($_POST["shutdown"])) {
	shutdown();
}

function shutdown() {
	exec('sudo shutdown -h now');
}

function print_shutdown() {
	if(is_cam_enabled() == '1') {
		$state = "Disable";
	}
	else {
		$state = "Enable";
	}
	echo '
    <!-- Die Encoding-Art enctype MUSS wie dargestellt angegeben werden -->
    <form enctype="multipart/form-data" action="?shutdown" method="POST">
        <!-- Der Name des Input Felds bestimmt den Namen im $_FILES Array -->
        <p><input type="submit" name="shutdown" value="Shutdown" /></p>
    </form>
    ';
}

?>
