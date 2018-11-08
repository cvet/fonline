pipeline {
  environment {
    FO_BUILD_DEST = 'Build'
    FO_SOURCE = '.'
    FO_FTP_DEST = '109.167.147.160'
    FO_INSTALL_PACKAGES = 0
  }
  agent none
  options {
    ansiColor('xterm')
  }

  stages {
    stage('Clean FTP directory') {
      options {
        skipDefaultCheckout true
      }
      agent {
        kubernetes {
          label 'linux'
          yamlFile 'BuildScripts/build-pod.yaml'
        }
      }
      steps {
        withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
          //sh './BuildScripts/cleanup.sh'
          sh 'echo 123'
        }
      }
    }
    stage('Build Main targets') {
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
              sh './BuildScripts/android.sh'
              stash name: 'androidbin', includes: '**/Build/android/**'
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
                sh './BuildScripts/linux.sh'
                stash name: 'androidbin', includes: '**/Build/linux/bin/**'
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
                sh './BuildScripts/web.sh'
                sh 'tree ./'
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
              bat 'BuildScripts\\windows.bat'
              stash name: 'androidbin', includes: '**/Build/windows/bin/**'
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
              label 'mac'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              sh './BuildScripts/mac.sh'
              stash name: 'androidbin', includes: '**/Build/mac/bin/**'
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

    stage('Upload build artifacts') {
      agent {
        node {
          label 'master'
        }
      }
      steps {
        withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
          sh 'tree ./'
          unstash 'linuxbin'
          sh 'tree ./'
          unstash 'androidbin'
          sh 'tree ./'
        }
      }
    }

  }
}
