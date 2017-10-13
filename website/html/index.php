<html>
<head>
<link rel="stylesheet" type="text/css" href="styles.css" />
</head>
<body>

<?php
include 'error.php';
include 'bitstream.php';
include 'upload.php';
?> 
<br>
<h2>Available Bitstreams:</h2>

<table>
  <thead>
    <tr><th>Name</th><th class="fixed"></th><th class="fixed"></th></tr>
  </thead>
  <tbody>
    <?php build_table(get_bitstream()); ?>
  </tbody>
</table>

<h2>Upload Bitstream:</h2>
<p><?php print_upload(); ?></p>
</body>
</html>
