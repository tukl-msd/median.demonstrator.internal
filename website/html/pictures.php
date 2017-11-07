<?php

if(isset($_POST["show_picture"])) {
    show_picture($_POST["show_picture"]);
}

function get_pictures() {   
    $bit = array();

    // Files in /home/median.demonstrator.internal/pictures/
    if ($handle = opendir('/home/median.demonstrator.internal/pictures/')) {
        
        // Einträge erstellen
        while (false !== ($entry = readdir($handle))) {
            if ($entry != '.' && $entry != '..') {
                $bit[] = $entry;
            }
        }
        
        closedir($handle);
    } 
    return $bit;
}

function build_table_pictures($array){
    // data rows
    foreach( $array as $key => $value){
        echo '<tr>';
        echo '<td>' . $value . '</td>';
        echo '<td>
          <form action="?show_picture" method="post">
            <input type="hidden" name="show_picture" value="'.$value.'">
            <input type="submit" value="Show">
          </form>
        </td>
        </tr>';
    }
}

function show_picture($name) {
	if(is_cam_enabled()[0] == '1') {
		exec('sudo /var/www/scripts/camera.sh 0');
	}
	
	
	$ret = exec('sudo cp /home/median.demonstrator.internal/pictures/' . $name . ' /sys/devices/platform/vio/picture');
}

?>