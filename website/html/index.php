<html>
<head>
<link rel="stylesheet" type="text/css" href="styles.css" />
</head>
<body>

<?php
include 'error.php';
include 'bitstream.php';
include 'upload.php';
include 'camera.php';
include 'pictures.php';
?> 
<br>
<h2>Available Bitstreams:</h2>

<table>
	<thead>
		<tr><th>Name</th><th class="fixed"></th><th class="fixed"></th></tr>
	</thead>
	<tbody>
		<?php build_table_bitstream(get_bitstream()); ?>
	</tbody>
</table>

<h2>Upload Bitstream:</h2>
<p><?php print_upload(); ?></p>

<h2>Camera:</h2>
<p><?php print_cam_enable(); ?></p>


<h2>Available Pictures:</h2>
<table>
	<thead>
		<tr><th>Name</th><th class="fixed"></th></tr>
	</thead>
	<tbody>
		<?php build_table_pictures(get_pictures()); ?>
	</tbody>
</table>
</body>
</html>
