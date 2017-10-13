<?php

function upload()
{
    $target_dir = "/var/www/uploads/";
    $target_file = $target_dir . basename($_FILES["fileToUpload"]["name"]);
    $uploadOk = 1;
    $imageFileType = pathinfo($target_file,PATHINFO_EXTENSION);
    
    // Check if file already exists
    //if (file_exists($target_file)) {
    //    echo "Sorry, file already exists.";
    //    $uploadOk = 0;
    //}
    // Check file size
    if ($_FILES["fileToUpload"]["size"] > 500000) {
        echo "Sorry, your file is too large.";
        $uploadOk = 0;
    }
    // Allow certain file formats
    if($imageFileType != "bit.bin") {
        echo "Only file type *.bit.bin allowed.<br>";
        $uploadOk = 0;
    }
    // Check if $uploadOk is set to 0 by an error
    if ($uploadOk == 0) {
        echo "Sorry, your file was not uploaded.";
    // if everything is ok, try to upload file
    } else {
        if (move_uploaded_file($_FILES["fileToUpload"]["tmp_name"], $target_file)) {
            echo "The file ". basename( $_FILES["fileToUpload"]["name"]). " has been uploaded.";
        } else {
            echo "Sorry, there was an error uploading your file.";
        }
    }
}

echo '<p>This is a PHP script</p>';

$uploaddir = '/var/www/uploads/';
$uploadfile = $uploaddir . basename($_FILES['userfile']['name']);

// Files in /lib/firmware
if ($handle = opendir('/lib/firmware')) {
    echo 'Verzeichnis-Handle: '. $handle .'<br>';
    echo "Einträge:\n";

    // Tabelle erstellen
    echo '<table><thead><tr><th>File</th><th></th></tr></thead><tbody><tr>';

    // Einträge erstellen
    while (false !== ($entry = readdir($handle))) {
        if ($entry != '.' && $entry != '..') {
            echo "<td>$entry</td><td></td>";
        }
    }
    
    echo '</tr></tbody></table>';

    closedir($handle);
} 

if (isset($_POST['userfile'])) {
  upload();
  echo '<script language="javascript">alert("message successfully sent")</script>';
}
else
{
    print_upload();
}

function print_upload() {
    echo '
    <!-- Die Encoding-Art enctype MUSS wie dargestellt angegeben werden -->
    <form enctype="multipart/form-data" action="" method="POST">
        <!-- MAX_FILE_SIZE muss vor dem Dateiupload Input Feld stehen -->
        <input type="hidden" name="MAX_FILE_SIZE" value="30000" />
        <!-- Der Name des Input Felds bestimmt den Namen im $_FILES Array -->
        Diese Datei hochladen: <input name="userfile" type="file" />
        <input type="submit" value="Send File" />
    </form>
    ';
}

?>