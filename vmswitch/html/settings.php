<!DOCTYPE html>
<html lang="en">

  <head>

    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
    <meta name="description" content="">
    <meta name="author" content="">

    <title>DPDK-based-Coding-Switch - Tables</title>

    <!--JQuery-->
    <script src="vendor/jquery/jquery.min.js"></script>

    <!-- Bootstrap core CSS-->
    <link href="vendor/bootstrap/css/bootstrap.min.css" rel="stylesheet">

    <!-- Custom fonts for this template-->
    <link href="vendor/fontawesome-free/css/all.min.css" rel="stylesheet" type="text/css">

    <!-- Page level plugin CSS-->
    <link href="vendor/datatables/dataTables.bootstrap4.css" rel="stylesheet">

    <!-- Custom styles for this template-->
    <link href="css/sb-admin.css" rel="stylesheet">

    <!--Import defined JS scripts-->
    <script src="helper.js"></script>>

  </head>

  <body id="page-top">

    <nav class="navbar navbar-expand navbar-dark bg-dark static-top">
      <a class="navbar-brand mr-1" href="index.php"><i class="fas fa-fw fa-server"></i> DPDK based coding switch</a>
    </nav>

    <div id="wrapper">
    
      <!-- Sidebar -->
      <ul class="sidebar navbar-nav">
        <li class="nav-item">
          <a class="nav-link" href="index.php">
            <i class="fas fa-fw fa-tachometer-alt"></i>
            <span>Dashboard</span></a>
        </li>
        <li class="nav-item active">
          <a class="nav-link" href="settings.php">
            <i class="fas fa-fw fa-cog"></i>
            <span>Settings</span></a>
        </li>
        <li class="nav-item">
          <a class="nav-link" href="logs.php">
            <i class="fas fa-fw fa-book"></i>
            <span>Logs</span></a>
        </li>
      </ul>

      <div id="content-wrapper">

        <div class="container-fluid">

          <div class="row">
           
            <div class="col-sm-6">

              <div class="card text-white bg-primary o-hidden h-5">
                <div class="card-body">
                  <div class="card-body-icon">
                    <i class="fas fa-fw fa-cogs"></i>
                  </div>
                  <div class="mr-5">General Settings</div>
                </div>
              </div>

              <br>

              <!-- Multiple Radios -->
              <div class="form-group">
                <label class="col-md-12 control-label" for="coding_enabled">Network Coding</label>
                <div class="col-md-12">
                  <div class="radio">
                    <label for="coding_enabled-0">
                      <input type="radio" name="coding_enabled" id="network_coding-1" value="1">
                      Yes
                    </label>
                  </div>
                  <div class="radio">
                    <label for="coding_enabled-1">
                      <input type="radio" name="coding_enabled" id="network_coding-0" value="2">
                      No
                    </label>
                  </div>
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="mac_entries">MAC_ENTRIES</label> 
                <div class="col-md-12">
                  <input id="mac_entries" name="mac_entries" type="text" class="form-control input-md">
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="nb_mbuf">NB_MBUF</label>  
                <div class="col-md-12">
                  <input id="nb_mbuf" name="nb_mbuf" type="text" class="form-control input-md">
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="max_pkt_burst">MAX_PKT_BURST</label>  
                <div class="col-md-12">
                  <input id="max_pkt_burst" name="max_pkt_burst" type="text" class="form-control input-md">
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="rte_test_rx_desc_default">RTE_TEST_RX_DESC_DEFAULT</label>  
                <div class="col-md-12">
                  <input id="rte_test_rx_desc_default" name="rte_test_rx_desc_default" type="text" class="form-control input-md">
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="rte_test_tx_desc_default">RTE_TEST_TX_DESC_DEFAULT</label>  
                <div class="col-md-12">
                  <input id="rte_test_tx_desc_default" name="rte_test_tx_desc_default" type="text" class="form-control input-md">
                </div>
              </div>
              
            </div>
            <!-- /.container-fluid -->

            <div class="col-sm-6">

              <div class="card text-white bg-primary o-hidden h-5">
                <div class="card-body">
                  <div class="card-body-icon">
                    <i class="fas fa-fw fa-sitemap"></i>
                  </div>
                  <div class="mr-5">Network Coding Settings</div>
                </div>
              </div>

              <br>

              <!-- Multiple Radios -->
              <div class="form-group">
                <label class="col-md-12 control-label" for="codec_select">Codec</label>
                <div class="col-md-12">
                <div class="radio">
                  <label for="codec_select-0">
                    <input type="radio" name="codec_select" id="codec-kodoc_full_vector" value="kodoc_full_vector">
                    Full vector
                  </label>
                </div>
                <div class="radio">
                  <label for="codec_select-1">
                    <input type="radio" name="codec_select" id="codec-kodoc_on_the_fly" value="kodoc_on_the_fly">
                    On the fly
                  </label>
                </div>
                <div class="radio">
                  <label for="codec_select-2">
                    <input type="radio" name="codec_select" id="codec-kodoc_sliding_window" value="kodoc_sliding_window">
                    Sliding window
                  </label>
                </div>
                </div>
              </div>

              <!-- Multiple Radios -->
              <div class="form-group">
                <label class="col-md-12 control-label" for="field_select">Finite Field</label>
                <div class="col-md-12">
                <div class="radio">
                  <label for="field_select-0">
                    <input type="radio" name="field_select" id="finite_field-kodoc_binary" value="kodoc_binary">
                    Binary
                  </label>
                </div>
                <div class="radio">
                  <label for="field_select-1">
                    <input type="radio" name="field_select" id="finite_field-kodoc_binary4" value="kodoc_binary4">
                    Binary4
                  </label>
                </div>
                <div class="radio">
                  <label for="field_select-2">
                    <input type="radio" name="field_select" id="finite_field-kodoc_binary8" value="kodoc_binary8">
                    Binary8
                  </label>
                </div>
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="max_symbols">MAX_SYMBOLS</label>  
                <div class="col-md-12">
                  <input id="max_symbols" name="max_symbols" type="text" class="form-control input-md"> 
                </div>
              </div>

              <!-- Text input-->
              <div class="form-group">
                <label class="col-md-12 control-label" for="max_symbol_size">MAX_SYMBOL_SIZE</label>  
                <div class="col-md-12">
                  <input id="max_symbol_size" name="max_symbol_size" type="text" class="form-control input-md"> 
                </div>
              </div>

            </div>
            <!-- /.container-fluid -->

            <div class="col-sm-12">
              <button id="btn_update_and_relaunch" type="button" class="btn btn-primary btn-block">Update and Relaunch DPDK Switch</button>
            </div>  <br><br>

            <!-- Sticky Footer -->
            <footer class="sticky-footer">
              <div class="container my-auto">
                <div class="copyright text-center my-auto">
                  <span>Daniel de Villiers (August 2018)</span>
                </div>
              </div>
            </footer>

          </div> 
        </div>  
        <!-- /.content-wrapper -->

      </div>
    </div>
    <!-- /#wrapper -->

    <!-- Scroll to Top Button-->
    <a class="scroll-to-top rounded" href="#page-top">
      <i class="fas fa-angle-up"></i>
    </a>

    <!-- Bootstrap core JavaScript-->
    <script src="vendor/jquery/jquery.min.js"></script>
    <script src="vendor/bootstrap/js/bootstrap.bundle.min.js"></script>

    <!-- Core plugin JavaScript-->
    <script src="vendor/jquery-easing/jquery.easing.min.js"></script>

    <!-- Page level plugin JavaScript-->
    <script src="vendor/datatables/jquery.dataTables.js"></script>
    <script src="vendor/datatables/dataTables.bootstrap4.js"></script>

    <!-- Custom scripts for all pages-->
    <script src="js/sb-admin.min.js"></script>

    <!-- Demo scripts for this page-->
    <script src="js/demo/datatables-demo.js"></script>

  </body>

</html>
