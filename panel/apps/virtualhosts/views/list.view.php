<?php defined('_EXE') or die('Restricted access'); ?>

<h2>Virtual Hosts</h2>
<?php if(count($virtualHosts)){ ?>
	<table class="table table-condensed">
		<thead>
			<tr>
				<td>Id</td>
				<td>Host</td>
				<td>Url</td>
				<td>Date</td>
			</tr>	
		</thead>
		<?php foreach($virtualHosts as $virtualHost){ ?>
			<tr>
				<td><?=$virtualHost->id?></td>
				<td><?=$virtualHost->host?></td>
				<td><?=$virtualHost->url?></td>
				<td><?=$virtualHost->dateAdd?></td>
			</tr>
		<?php } ?>
	</table>
<?php }else{ ?>
	<blockquote>
		No data
	</blockquote>
<?php } ?>