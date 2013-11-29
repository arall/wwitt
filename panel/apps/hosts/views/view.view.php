<?php defined('_EXE') or die('Restricted access'); ?>

<h2>Details</h2>
<form class="form-horizontal" role="form">
	<div class="form-group">
		<label class="col-sm-2 control-label">IP</label>
		<div class="col-sm-10">
			<?=$host->ipAdress?>
		</div>
	</div>
	<div class="form-group">
		<label class="col-sm-2 control-label">Status</label>
		<div class="col-sm-10">
			<?=$host->status?>
		</div>
	</div>
	<div class="form-group">
		<label class="col-sm-2 control-label">Hostname</label>
		<div class="col-sm-10">
			<?=$host->hostname?>
		</div>
	</div>
	<div class="form-group">
		<label class="col-sm-2 control-label">Operative System</label>
		<div class="col-sm-10">
			<?=$host->os?>
		</div>
	</div>
	<div class="form-group">
		<label class="col-sm-2 control-label">Date Added</label>
		<div class="col-sm-10">
			<?=$host->dateAdd?>
		</div>
	</div>
	<div class="form-group">
		<label class="col-sm-2 control-label">Date Updated</label>
		<div class="col-sm-10">
			<?=$host->dateUpdate?>
		</div>
	</div>
</form>

<?php if($services){ ?>
	<h2>Services</h2>
	<table class="table table-condensed">
		<thead>
			<tr>
				<td>Id</td>
				<td>Port</td>
				<td>Status</td>
				<td>Head</td>
				<td>Name</td>
				<td>Protocol</td>
				<td>Product</td>
				<td>Version</td>
				<td>Info</td>
				<td>Date</td>
				<td></td>
			</tr>	
		</thead>
		<?php foreach($services as $service){ ?>
			<tr>
				<td><?=$service->id?></td>
				<td><?=$service->port?></td>
				<td><?=$service->getStatusToString();?></td>
				<td><?=$service->head?></td>
				<td><?=$service->name?></td>
				<td><?=$service->protocol?></td>
				<td><?=$service->product?></td>
				<td><?=$service->version?></td>
				<td><?=$service->info?></td>
				<td><?=$service->dateAdd?></td>
				<td>
					<div class="btn-group">
						<button type="button" class="btn btn-danger">Exploit</button>
						<button type="button" class="btn btn-danger dropdown-toggle" data-toggle="dropdown">
							<span class="caret"></span>
							<span class="sr-only">Toggle Dropdown</span>
						</button>
						<ul class="dropdown-menu" role="menu">
							<?php foreach($vulns as $vuln){ ?>
								<li>
									<a href="#" onClick="exploit(<?=$service->port?>, <?=$vuln->id?>);">
										<?=$vuln->name?>
									</a>
								</li>
							<?php } ?>
						</ul>
					</div>
				</td>
			</tr>
		<?php } ?>
	</table>
<?php } ?>

<?php if($virtualHosts){ ?>
	<h2>Virtual Hosts</h2>
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
<?php } ?>

<script>
function exploit(port, vulnId){
	window.location.href = "<?=Url::site();?>hosts/exploit?ip=<?=$host->ip?>&port=" + port + "&vulnId=" + vulnId;
}
</script>