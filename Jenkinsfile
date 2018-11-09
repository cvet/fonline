pipeline {
  environment {
    FO_BUILD_DEST = 'Build'
    FO_SOURCE = '.'
    FO_INSTALL_PACKAGES = 0
  }
  agent none

  stages {
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
              sh './BuildScripts/android.sh'
              stash name: 'android', includes: '**/Build/android/bin/**'
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
                sh './BuildScripts/linux.sh'
                stash name: 'linux', includes: '**/Build/linux/bin/**'
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
                sh './BuildScripts/web.sh'
                stash name: 'web', includes: '**/Build/web/bin/**'
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
              bat 'BuildScripts\\windows.bat'
              stash name: 'windows', includes: '**/Build/windows/bin/**'
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
              sh './BuildScripts/mac.sh'
              stash name: 'macos', includes: '**/Build/mac/bin/**'
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
	
	
	
	
	
	    post {
	success {
		node('master') {
			unstash 'linux'
			unstash 'android'
			unstash 'windows'
			unstash 'macos'
			unstash 'web'
			sh 'tree ./'
			sh 'zip -r -0 $GIT_COMMIT.zip ./'
			sh "tree ./"
			archiveArtifacts artifacts: "${GIT_COMMIT}.zip", fingerprint: true
		}
        }
    }	
	
	
	
	
	
	
}
