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
            dir('Build/android/'){
                stash name: 'android', includes: 'Binaries/**'
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
              sh './BuildScripts/linux.sh'
              sh 'tree ./'
              dir('Build/linux/'){
                  stash name: 'linux', includes: 'Binaries/**'
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
              sh './BuildScripts/web.sh'
              sh 'tree ./'
              dir('Build/web/'){
                  stash name: 'web', includes: 'Binaries/**'
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
            bat 'BuildScripts\\windows.bat'
            dir('Build/windows/'){
                stash name: 'windows', includes: 'Binaries/**'
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
            sh './BuildScripts/mac.sh'
            sh 'ls -la ./'
            dir('Build/mac/'){
                stash name: 'mac', includes: 'Binaries/**'
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

    stage('Assemble build artifacts') {

      agent {
        node {
          label 'master'
        }
      }
      steps {
        sh 'mkdir -p release'
        sh 'mv SDK/* ./release/'
        dir('release')
        {
          unstash 'linux'
          unstash 'android'
          unstash 'windows'
          unstash 'mac'
          unstash 'web'
          sh 'tree ./'
          sh 'zip -r -0 $GIT_COMMIT.zip ./'
          sh 'tree ./'
        }
      }
      post {
        success{
          dir('release'){
              archiveArtifacts artifacts: "$GIT_COMMIT.zip", fingerprint: true
          }
        }
      }
    }

  }

}
