<?php defined('_EXE') or die('Restricted access'); ?>

<h1>
	<span class="glyphicon glyphicon-globe"></span>
	Virtual Hosts
</h1>
<br>

<form method="post" action="<?=Url::site("virtualHosts")?>">
	<?php if(count($virtualHosts)){ ?>
		<div class="table-responsive">
			<table class="table table-striped">
				<thead>
					<tr>
						<td>Host</td>
						<td>Url</td>
						<td>Date</td>
					</tr>	
				</thead>
				<tbody>
				<?php foreach($virtualHosts as $virtualHost){ ?>
					<tr>
						<td><?=$virtualHost->host?></td>
						<td>
							<a href="<?=$virtualHost->url?>" taget="_blank">
								<?=$virtualHost->url?>
							</a>
						</td>
						<td><?=Helper::humanDate($virtualHost->dateInsert)?></td>
					</tr>
				<?php } ?>
				</tbody>
			</table>
			<?php $controller->setData("pag", $pag); ?>
			<?=$controller->view("modules.pagination");?>
		</div>
	<?php }else{ ?>
		<blockquote>
			No data
		</blockquote>
	<?php } ?>
</form>