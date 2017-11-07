<?php

if(isset($_POST["cam_toggle"])) {
	cam_toggle();
}

function is_cam_enabled() {
    $ret = exec("sudo cat /sys/devices/platform/vio/pi_cam");
	return $ret;
}

function cam_toggle() {
	if(is_cam_enabled()[0] == '1') {
		exec('sudo /var/www/scripts/camera.sh 0');
	}
	else {
		exec('sudo /var/www/scripts/camera.sh 1');
	}
}

function print_cam_enable() {
	if(is_cam_enabled() == '1') {
		$state = "Disable";
	}
	else {
		$state = "Enable";
	}
	echo '
    <!-- Die Encoding-Art enctype MUSS wie dargestellt angegeben werden -->
    <form enctype="multipart/form-data" action="?cam_toggle" method="POST">
        <!-- Der Name des Input Felds bestimmt den Namen im $_FILES Array -->
        <p><input type="submit" name="cam_toggle" value="'.$state.' Cam" /></p>
    </form>
    ';
}

?>
