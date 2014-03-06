<?php defined('_EXE') or die('Restricted access'); ?>

<h1>
	<span class="glyphicon glyphicon-tasks"></span>
	Hosts
</h1>
<br>

<form method="post" action="<?=Url::site("hosts")?>">
	<?php if(count($hosts)){ ?>
		<div class="table-responsive">
			<table class="table table-striped">
				<thead>
					<tr>
						<td>IP</td>
						<td>Status</td>
						<td>Hostname</td>
						<td>OS</td>
						<td>Services</td>
						<td>Virtual Hosts</td>
						<td>Date</td>
					</tr>	
				</thead>
				<tbody>
				<?php foreach($hosts as $host){ ?>
					<tr>
						<td>
							<a href="<?=Url::site("hosts/details/".$host->ip)?>">
								<?=$host->ipAdress?>
							</a>
						</td>
						<td>
							<span class="label label-<?=$host->getStatusCssString();?>">
								<?=$host->getStatusString();?>
							</span>
						</td>
						<td><?=$host->hostname?></td>
						<td><?=$host->os?></td>
						<td><?=$host->totalServices?></td>
						<td><?=$host->totalVirtualhosts?></td>
						<td><?=Helper::humanDate($host->dateInsert)?></td>
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