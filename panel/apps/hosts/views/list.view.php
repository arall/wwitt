<?php defined('_EXE') or die('Restricted access'); ?>

<h2>Hosts</h2>
<?php if(count($hosts)){ ?>
	<table class="table table-condensed">
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
		<?php foreach($hosts as $host){ ?>
			<tr>
				<td>
					<a href="<?=Url::site("hosts/details/".$host->ip)?>">
						<?=$host->ipAdress?>
					</a>
				</td>
				<td><?=$host->status?></td>
				<td><?=$host->hostname?></td>
				<td><?=$host->os?></td>
				<td><?=$host->totalServices?></td>
				<td><?=$host->totalVirtualhosts?></td>
				<td><?=$host->dateAdd?></td>
			</tr>
		<?php } ?>
	</table>
<?php }else{ ?>
	<blockquote>
		No data
	</blockquote>
<?php } ?>
