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
            sh 'tree ./'
            stash name: 'android', includes: 'Build/android/Binaries/**'
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
              sh 'tree ./'
              stash name: 'linux', includes: 'Build/linux/Binaries/**'
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
              sh 'tree ./'
              stash name: 'web', includes: 'Build/web/Binaries/**'
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
            bat 'dir'
            stash name: 'windows', includes: 'Build/windows/Binaries/**'
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
            sh 'ls -la ./'
            stash name: 'macos', includes: 'Build/mac/Binaries/**'
          }
          post {
            cleanup{
              deleteDir()
            }
          }
        }
      }
    }

    stage('Build Mac OS') {

      agent {
        node {
          label 'master'
        }
      }
      steps {
        deleteDir()
        unstash 'linux'
        unstash 'android'
        unstash 'windows'
        unstash 'macos'
        unstash 'web'
        sh 'tree ./'
        sh 'zip -r -0 build.zip ./'
        sh "tree ./"
      }
      post {
        success{
          archiveArtifacts artifacts: "build.zip", fingerprint: true
        }
      }
    }

  }

}
