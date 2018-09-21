import sys
from PyQt5.QtWidgets import QDialog, QApplication
from switch_tester_gui import Ui_Dialog

class TestSettings(QDialog):
    def __init__(self):
        super(TestSettings, self).__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)
        self.show()

class AppWindow(QDialog):
    def __init__(self):
        super(AppWindow, self).__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)
        self.show()
        self.ui.btn_testSettings.clicked.connect(self.executeTestSettings)

    def executeTestSettings(self):  
        test_settings = TestSettings() 
        test_settings.exec_()

app = QApplication(sys.argv)
w = AppWindow()
w.show()
sys.exit(app.exec_())