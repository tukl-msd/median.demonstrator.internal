<?php

if(isset($_POST["start_bitstream"])) {
    start_bitstream($_POST["start_bitstream"]);
    // header("Location: " . $_SERVER['REQUEST_URI']);
}

if(isset($_POST["delete_bitstream"])) {
    delete_bitstream($_POST["delete_bitstream"]);
    // header("Location: " . $_SERVER['REQUEST_URI']);
}

function get_bitstream() {   
    $bit = array();

    // Files in /lib/firmware
    if ($handle = opendir('/lib/firmware')) {
        
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

function build_table($array){
    // data rows
    foreach( $array as $key => $value){
        echo '<tr>';
        echo '<td>' . $value . '</td>';
        echo '<td>
          <form action="?start_bitstream" method="post">
            <input type="hidden" name="start_bitstream" value="'.$value.'">
            <input type="submit" value="Load to FPGA">
          </form>
        </td>';
        echo '<td>
          <form action="?delete_bitstream" method="post">
            <input type="hidden" name="delete_bitstream" id ="delete_bitstream" value="'.$value.'">';
            if(check_link($value)){    
                echo '<input type="submit" value="Delete" disabled>';
            }
            else {
                echo '<input type="submit" value="Delete" >';        
            }
        echo '</form>
        </td>
        </tr>';
    }
}

function start_bitstream($name) {
 // echo '/var/www/scripts/load_bitstream.sh ' . $name;
  $ret = exec('/var/www/scripts/load_bitstream.sh ' . $name);
 // echo $ret;
  if ($ret != 'operating') {
    echo '<script language="javascript">alert("Error loading bitstream '.$name.' - '.$ret.'")</script>';
  }
}

function delete_bitstream($name) {
  // echo $name.'<br>';
  // echo 'rm /lib/firmware/' . $name;
  $ret = exec('rm /lib/firmware/' . $name);
  // echo $ret;
  if ($ret != '') {
    echo '<script language="javascript">alert("Error deleting bitstream '.$name.' - '.$ret.'")</script>';
  }
}

function check_link($name) {
  $ret = exec('ls -l /lib/firmware/' . $name);
  if($ret[0] == 'l') {
    # Link
    return true;
  }
  return false;
}

?>