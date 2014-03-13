<?php defined('_EXE') or die('Restricted access'); ?>

<h1>
	<span class="glyphicon glyphicon-tasks"></span>
	Hosts
	<small><?=$host->ipAdress?></small>
</h1>
<br>

<div class="row">
	<div class="col-md-12">
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
									<?php $vulns = Vuln::select(
										array(
											"protocol" => $service->protocol
										)
									); ?>
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
													<a href="#" onClick="exploit(<?=$service->port?>, '<?=$vuln->module?>');">
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
function exploit(port, vuln){
	window.location.href = "<?=Url::site();?>hosts/exploit?ip=<?=$host->ip?>&port=" + port + "&vuln=" + vuln;
}
</script>