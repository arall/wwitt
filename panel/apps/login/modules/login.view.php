<?php defined('_EXE') or die('Restricted access'); ?>

<div class="row">
	<div class="col-md-offset-4 col-md-4">
		<div class="row well">
			<fieldset>
				<legend>
					Login
				</legend>
				<?php $user = Registry::getUser(); ?>
				<form class="form-horizontal ajax" role="form" method="post" name="loginForm" id="loginForm" action="<?=Url::site("login/doLogin")?>">
					<!-- Username -->
					<div class="form-group">
					    <div class="col-sm-12">
					    	<input type="text" class="form-control" id="login" name="login" placeholder="Username">
					    </div>
					</div>
					<!-- Password -->
					<div class="form-group">
					    <div class="col-sm-12">
					    	<input type="password" class="form-control" id="password" name="password" placeholder="Password">
					    </div>
					</div>
					<!-- Buttons -->
					<div class="form-group">
					    <div class="col-sm-12">
					    	<button class="btn btn-primary ladda-button btn-block" data-style="slide-left">
								<span class="ladda-label">
					    			Login
					    		</span>
					    	</button>
					    </div>
					</div>
				</form>
			</fieldset>
		</div>
	</div>
</div>