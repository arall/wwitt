<?php defined('_EXE') or die('Restricted access'); ?>

<h1>
	<span class="glyphicon glyphicon-tasks"></span>
	Hosts
	<small>Details</small>
</h1>
<br>

<div class="row">
	<div class="col-md-4">
		<div class="panel panel-default">
			<div class="panel-heading">
				<span class="glyphicon glyphicon-tasks"></span>
				Details
			</div>
		  	<div class="panel-body">
		  		<form method="post" id="mainForm" action="<?=Url::site();?>" class="form-horizontal ajax" role="form" autocomplete="off">
					<input type="hidden" name="app" id="app" value="hub">
					<input type="hidden" name="action" id="action" value="attack">
			    	<div class="form-group">
						<label for="host" class="col-sm-4 control-label">
							IP
						</label>
						<div class="col-sm-8">
							<p class="form-control-static">
								<?=$host->ipAdress?>
							<p>
						</div>
					</div>
					<div class="form-group">
						<label for="host" class="col-sm-4 control-label">
							Status
						</label>
						<div class="col-sm-8">
							<p class="form-control-static">
								<?=$host->status?>
							<p>
						</div>
					</div>
					<div class="form-group">
						<label for="host" class="col-sm-4 control-label">
							Date Added
						</label>
						<div class="col-sm-8">
							<p class="form-control-static">
								<?=Helper::humanDate($host->dateInsert)?>
							<p>
						</div>
					</div>
					<div class="form-group">
						<label for="host" class="col-sm-4 control-label">
							Date Updated
						</label>
						<div class="col-sm-8">
							<p class="form-control-static">
								<?=Helper::humanDate($host->dateUpdate)?>
							<p>
						</div>
					</div>
				</form>
			</div>
		</div>
	</div>
	<div class="col-md-8">
		<?php if($services){ ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<span class="glyphicon glyphicon-leaf"></span>
				 Services
			</div>
		  	<div class="panel-body">
		  		<div class="table-responsive">
					<table class="table table-striped">
						<thead>
							<tr>
								<td>Port</td>
								<td>Status</td>
								<td>Protocol</td>
								<td>Product</td>
								<td>Version</td>
								<td>Date</td>
								<td></td>
							</tr>	
						</thead>
						<tbody>
						<?php foreach($services as $service){ ?>
							<tr>
								<td><?=$service->port?></td>
								<td>
									<span class="label label-<?=$service->getFilteredCssString();?>">
										<?=$service->getFilteredString();?>
									</span>
								</td>
								<td><?=$service->protocol?></td>
								<td><?=$service->product?></td>
								<td><?=$service->version?></td>
								<td><?=Helper::humanDate($service->dateInsert)?></td>
								<td>
									<?php $vulns = Vuln::selectVulns($service->port); ?>
									<?php if(count($vulns)){ ?>
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
									<?php } ?>
								</td>
							</tr>
						<?php } ?>
						</tbody>
					</table>
				</div>
		  	</div>
		</div>
		<?php } ?>
	</div>
</div>

<script>
function exploit(port, vulnId){
	window.location.href = "<?=Url::site();?>hosts/exploit?ip=<?=$host->ip?>&port=" + port + "&vulnId=" + vulnId;
}
</script>