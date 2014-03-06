<!DOCTYPE html>
<html lang="en">
	<head>
		<meta charset="utf-8">
		<meta name="viewport" content="width=device-width, initial-scale=1.0">
		<title>WWITT</title>
		<link href="<?=Url::template("css/bootstrap.min.css");?>" media="screen" rel="stylesheet" type="text/css" />
		<link href="<?=Url::template("css/bootstrap-switch.min.css");?>" media="screen" rel="stylesheet" type="text/css" />
		<link href="<?=Url::template("css/ladda-themeless.min.css");?>" media="screen" rel="stylesheet" type="text/css" />
		<link href="<?=Url::template("css/custom.css");?>" media="screen" rel="stylesheet" type="text/css" />
		<!-- HTML5 Shim and Respond.js IE8 support of HTML5 elements and media queries -->
	    <!-- WARNING: Respond.js doesn't work if you view the page via file:// -->
	    <!--[if lt IE 9]>
	      <script src="https://oss.maxcdn.com/libs/html5shiv/3.7.0/html5shiv.js"></script>
	      <script src="https://oss.maxcdn.com/libs/respond.js/1.3.0/respond.min.js"></script>
	    <![endif]-->
		<script src="<?=Url::template("js/jquery-2.1.0.min.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/bootstrap.min.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/jquery.forms.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/bootstrap-switch.min.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/spin.min.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/ladda.min.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/jquery.numeric.js");?>" type="text/javascript"></script>
		<script src="<?=Url::template("js/init.js");?>" type="text/javascript"></script>
		<link rel="shortcut icon" href="<?=Url::template("img/favicon.png")?>">
	</head>
	<body>
		<div id="wrap">
			<?php $user = Registry::getUser(); ?>
			<?php if($user->id){ ?>
				<?=$controller->view("modules.topMenu");?>
			<?php } ?>
			<div class="container">
				<?php $messages = Registry::getMessages(); ?>
				<div id="mensajes-sys">
				<?php if($messages){ ?>
					<?php foreach($messages as $message){ ?>
						<?php if($message->message){ ?>
							<div class="alert alert-<?=$message->type?>">
								<button type="button" class="close" data-dismiss="alert">&times;</button>
								<?=$message->message?>
							</div>
						<?php } ?>
					<?php } ?>
				<?php } ?>
				</div>
				<?=$content?>
	      	</div>
	   	</div>
	</body>
</html>