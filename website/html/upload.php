<?php

if(isset($_POST["upload"])) {
    //upload();
    if($_FILES["fileToUpload"] == "") {
        echo "empty";
    } else {
        $target_file = basename($_FILES["fileToUpload"]["name"]);
        //echo $target_file . "</br>";
        check_upload();
    }
    // header("Location: " . $_SERVER['REQUEST_URI']);
}

function check_upload() {
    $target_dir = "/lib/firmware/";
    $target_file = $target_dir . basename($_FILES["fileToUpload"]["name"]);
    $uploadOk = 1;
    $BinFileType = pathinfo($target_file,PATHINFO_EXTENSION);
    $BitFileType = pathinfo(pathinfo($target_file,  PATHINFO_FILENAME), PATHINFO_EXTENSION);
    
    // Check if file already exists
    //if (file_exists($target_file)) {
    //    echo "Sorry, file already exists.";
    //    $uploadOk = 0;
    //}
    // Check file size
    if ($_FILES["fileToUpload"]["size"] > 10000000) {
        echo "Sorry, your file is too large.";
        $uploadOk = 0;
    }
    // Allow certain file formats
    if($BinFileType != "bin" && $BitFileType != "bit") {
        echo "Only file type *.bit.bin allowed.<br>";
        $uploadOk = 0;
    }
    // Check if $uploadOk is set to 0 by an error
    if ($uploadOk == 0) {
        echo "Sorry, your file was not uploaded.";
    // if everything is ok, try to upload file
    } else {
        if (move_uploaded_file($_FILES["fileToUpload"]["tmp_name"], $target_file)) {
            //echo "The file ". basename( $_FILES["fileToUpload"]["name"]). " has been uploaded.";
        } else {
            echo "Sorry, there was an error uploading your file.";
        }
    }
}

function print_upload() {
    echo '
    <!-- Die Encoding-Art enctype MUSS wie dargestellt angegeben werden -->
    <form enctype="multipart/form-data" action="?upload" method="POST">
        <!-- MAX_FILE_SIZE muss vor dem Dateiupload Input Feld stehen -->
        <input type="hidden" name="MAX_FILE_SIZE" value="100000000" />
        <!-- Der Name des Input Felds bestimmt den Namen im $_FILES Array -->
        <p>Diese Datei hochladen: <input name="fileToUpload" type="file" id="fileToUpload" /></p>
        <p><input type="submit" name="upload" value="Upload File" /></p>
    </form>
    ';
}

?>
