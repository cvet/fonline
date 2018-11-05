pipeline {
  environment {
    FO_BUILD_DEST = 'Build'
    FO_SOURCE = '.'
    FO_FTP_DEST = '109.167.147.160'
    FO_INSTALL_PACKAGES = 0
  }
  agent none
  options { skipDefaultCheckout() }

  stages {
    stage('Clean FTP directory') {
      agent {
        kubernetes {
          label 'linux'
          yamlFile 'BuildScripts/build-pod.yaml'
        }
      }
      steps {
        withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
          //checkout scm
          sh 'chmod +x ./BuildScripts/cleanup.sh'
          //sh './BuildScripts/cleanup.sh'
        }
      }
    }
    stage('Build') {
      parallel {
        stage('Build Android') {
          agent {
            kubernetes {
              label 'linux'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              checkout scm
              sh 'chmod +x ./BuildScripts/android.sh'
              sh './BuildScripts/android.sh'
            }
          }
        }
        stage('Build Linux') {
          agent {
            kubernetes {
              label 'linux'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                checkout scm
                sh 'chmod +x ./BuildScripts/linux.sh'
                sh './BuildScripts/linux.sh'
              }
            }
          }
        }
        stage('Build Web') {
          agent {
            kubernetes {
              label 'linux'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                checkout scm
                sh 'chmod +x ./BuildScripts/web.sh'
                sh './BuildScripts/web.sh'
              }
            }
          }
        }
        stage('Build Windows') {
          agent {
            node {
              label 'win'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              checkout scm
              bat 'BuildScripts\\windows.bat'
            }
          }
					post {
    				cleanup{
        			deleteDir()
    				}
					}
        }
        stage('Build Mac OS') {
          agent {
            node {
              label 'mac1.ci.fonline.ru'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              checkout scm
              sh 'chmod +x ./BuildScripts/mac.sh'
              sh './BuildScripts/mac.sh'
            }
          }
					post {
    				cleanup{
        			deleteDir()
    				}
					}
        }
      }
    }
  }
}
