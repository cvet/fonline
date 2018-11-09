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
          sh 'mkdir ./keklel'
          sh 'echo 1234 >> ./keklel/lel.txt'
          stash name: 'keklel', includes: '**'
        }
      }
    }
    stage('Build Main targets') {
      parallel {
        stage('Build Android') {
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
              //sh './BuildScripts/android.sh'
              //stash name: 'android', includes: '**/Build/android/bin/**'
              print '123'
            }
          }
        }
        stage('Build Linux') {
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
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                //sh './BuildScripts/linux.sh'
                //stash name: 'linux', includes: '**/Build/linux/bin/**'
                print '123'
              }
            }
          }
        }
        stage('Build Web') {
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
            container('jnlp') {
              withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
                //sh './BuildScripts/web.sh'
                //stash name: 'web', includes: '**/Build/web/bin/**'
                print '123'
              }
            }
          }
        }
        stage('Build Windows') {
          options {
        		skipDefaultCheckout true
      		}
          agent {
            node {
              label 'win'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              //bat 'BuildScripts\\windows.bat'
              //stash name: 'windows', includes: '**/Build/windows/bin/**'
              print '123'
            }
          }
					post {
    				cleanup{
        			deleteDir()
    				}
					}
        }
        stage('Build Mac OS') {
          options {
        		skipDefaultCheckout true
      		}
          agent {
            node {
              label 'mac'
            }
          }
          steps {
            withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
              //sh './BuildScripts/mac.sh'
              //stash name: 'macos', includes: '**/Build/mac/bin/**'
              print '123'
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
      //options {
      //  skipDefaultCheckout true
      //}      
      steps {
        withCredentials(bindings: [string(credentialsId: '0d28d996-7f62-49a2-b647-8f5bfc89a661', variable: 'FO_FTP_USER')]) {
          //unstash 'linux'
          //unstash 'android'
          //unstash 'windows'
          //unstash 'macos'
          //unstash 'web'
          //ls 'tree ./'
	  unstash 'keklel'
          sh 'tree ./'
	  sh "zip -r $GIT_COMMIT.zip ./"
	  sh 'tree ./'
		nexusArtifactUploader artifacts: [[artifactId: "$GIT_COMMIT", classifier: '', file: "$GIT_COMMIT", type: 'zip']], credentialsId: 'b8c7457a-3141-4e6b-aff8-8d1e18a52248', groupId: 'dd', nexusUrl: 'builds.fonline.ru', nexusVersion: 'nexus3', protocol: 'https', repository: 'sdk', version: "GIT_COMMIT"
        }
      }
    }

  }
}
