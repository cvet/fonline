pipeline {
  environment {
    FO_BUILD_DEST = 'Build'
    FO_SOURCE = '.'
    FO_FTP_DEST = '109.167.147.160'
    FO_INSTALL_PACKAGES = 0
  }
  agent {
    node {
      label 'master'
    }
  }

  stages {
    stage('Clean FTP directory') {
      options {
        skipDefaultCheckout()
      }
      agent {
        node {
          label 'master'
        }
      }
      steps {
        withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
          sh './BuildScripts/cleanup.sh'
        }
      }
    }  
    //stage('Build') {
      parallel {
        stage('Build Android') {
          agent {
            kubernetes {
              label 'fonline-sdk'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              sh './BuildScripts/android.sh'
            }
          }
        }
        stage('Build iOS') {
          agent {
            node {
              label 'master'
            }
          }
          steps { 
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              sh './BuildScripts/ios.sh'
            }
          }
					post {
    				cleanup{
        			deleteDir()
    				}
					}          
        }        
        stage('Build Linux') {
          agent {
            kubernetes {
              label 'fonline-sdk'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                sh './BuildScripts/linux.sh'
              }
            }
          }
        }
        stage('Build Web') {
          agent {
            kubernetes {
              label 'fonline-sdk'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                sh './BuildScripts/web.sh'
              }
            }
          }
        }
        stage('Build Windows') {
          agent {
            node {
              label 'win1.ci.fonline.ru'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
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
    //}
  }
}
