<!-- Template Modified by DBB de Villiers (August 2018) from: https://startbootstrap.com/template-overviews/sb-admin/-->
<!DOCTYPE html>
<html lang="en">

  <head>

    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
    <meta name="description" content="">
    <meta name="author" content="">

    <title>DPDK-based-Coding-Switch - Dashboard</title>

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
        <li class="nav-item active">
          <a class="nav-link" href="index.php">
            <i class="fas fa-fw fa-tachometer-alt"></i>
            <span>Dashboard</span></a>
        </li>
        <li class="nav-item">
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

          <!-- Icon Cards-->
          <div class="row">
            <div class="col-xl-3 col-sm-6 mb-3">
              <div class="card text-white bg-primary o-hidden h-100">
                <div class="card-body">
                  <div class="card-body-icon">
                    <i class="fas fa-fw fa-download"></i>
                  </div>
                  <div class="mr-5">Rx</div>
                </div>
              </div>
            </div>
            <div class="col-xl-3 col-sm-6 mb-3">
              <div class="card text-white bg-primary o-hidden h-100">
                <div class="card-body">
                  <div class="card-body-icon">
                    <i class="fas fa-fw fa-upload"></i>
                  </div>
                  <div class="mr-5">Tx</div>
                </div>
              </div>
            </div>
            <div class="col-xl-3 col-sm-6 mb-3">
              <div class="card text-white bg-primary o-hidden h-100">
                <div class="card-body">
                  <div class="card-body-icon">
                    <i class="fas fa-fw fa-microchip"></i>
                  </div>
                  <div class="mr-5">CPU and Memory usage:</div>
                </div>
              </div>
            </div>
            <div class="col-xl-3 col-sm-6 mb-3">
              <div class="card text-white bg-primary o-hidden h-100">
                <div class="card-body">
                  <div class="card-body-icon">
                    <i class="fas fa-fw fa-history"></i>
                  </div>
                  <div class="mr-5">Time active</div>
                </div>
              </div>
            </div>
          </div>

          <table class="table">
            <thead>
              <tr>
                <th scope="col">PORT</th>
                <th scope="col">STATUS</th>
                <th scope="col">IPv4 Address</th>
                <th scope="col">MAC Address</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td>0</td>
                <td>DOWN</td>
                <td>192.168.1.8</td>
                <td>DE:AD:BE:EF:00:02</td>
              </tr>
            </tbody>
          </table>

        <button id="btn_reboot" type="button" class="btn btn-primary btn-block">Reboot</button> <br><br>

        <!-- Sticky Footer -->
        <footer class="sticky-footer">
          <div class="container my-auto">
            <div class="copyright text-center my-auto">
              <span>Daniel de Villiers (August 2018)</span>
            </div>
          </div>
        </footer>

      </div>
      <!-- /.content-wrapper -->

    </div>
    <!-- /#wrapper -->

    <!-- Scroll to Top Button-->
    <a class="scroll-to-top rounded" href="#page-top">
      <i class="fas fa-angle-up"></i>
    </a>

    <!-- Bootstrap core JavaScript-->
    <script src="vendor/bootstrap/js/bootstrap.bundle.min.js"></script>

    <!-- Core plugin JavaScript-->
    <script src="vendor/jquery-easing/jquery.easing.min.js"></script>

    <!-- Custom scripts for all pages-->
    <script src="js/sb-admin.min.js"></script>

  </body>

</html>
