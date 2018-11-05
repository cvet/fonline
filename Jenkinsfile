pipeline {
  environment {
    FO_BUILD_DEST = 'Build'
    FO_SOURCE = '.'
    FO_FTP_DEST = '109.167.147.160'
    FO_INSTALL_PACKAGES = 0
  }
  agent none
  stages {
    stage('Build') {
      parallel {
        stage('Build Android') {
          agent {
            label 'linux'
            kubernetes {
              label 'fonline-sdk'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                sh 'chmod +x ./BuildScripts/android.sh'
                sh './BuildScripts/android.sh'
              }
            }
          }
        }
        stage('Build Linux') {
          agent {
            label 'linux'
            kubernetes {
              label 'fonline-sdk'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                sh 'chmod +x ./BuildScripts/linux.sh'
                sh './BuildScripts/linux.sh'
              }
            }
          }
        }
        stage('Build Web') {
          agent {
            label 'linux'
            kubernetes {
              label 'fonline-sdk'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                sh 'chmod +x ./BuildScripts/web.sh'
                sh './BuildScripts/web.sh'
              }
            }
          }
        }
        stage('Build Mac') {
          agent {
            label 'mac'
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              sh 'chmod +x ./BuildScripts/mac.sh'
              sh './BuildScripts/mac.sh'
            }
          }
        }
        stage('Build Windows') {
          agent {
            label 'win'
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              bat 'cmd.exe /c BuildScripts\\windows.bat'
            }
          }
        }
      }
    }
  }
}
