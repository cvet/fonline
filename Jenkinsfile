pipeline {
  environment {
    FO_INSTALL_PACKAGES = 0
  }
  options {
      disableConcurrentBuilds()
  }
  agent none
  stages {
    stage('Build Main Targets') {
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
          }
          post {
            cleanup {
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
          }
          post {
            cleanup {
              deleteDir()
            }
          }
        }
        stage('Build iOS') {
          agent {
            node {
              label 'mac'
            }
          }
          steps {
            sh './BuildScripts/ios.sh'
          }
          post {
            cleanup {
              deleteDir()
            }
          }
        }
      }
    }
  }
}
